#include "BlvckWinPipe/Server/Pipes/PipeListener.h"

#include <cassert>
#include <stdexcept>

#include "BlvckWinPipe/Utils/WinUtils.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    /**
     * @brief Posts an asynchronous accept operation for a new client connection.
     *
     * Creates a new named pipe instance, registers it with the IO completion port (`_IOCP`),
     * and initiates an overlapped ConnectNamedPipe operation. On success, the handle
     * is stored in `_PipeHandle` and `_PendingOps` is incremented to track the pending operation.
     *
     * If any step fails due to a **transient/recoverable error** (e.g., low system resources,
     * pipe busy, or temporary kernel exhaustion), the function returns `false` so the caller
     * can retry later. Permanent errors (e.g., invalid parameters) throw a `std::runtime_error`.
     *
     * @note The function assumes `_IsRunning` is `true` to proceed. If `_PipeHandle` is
     *       already valid, no new pipe is created and the function returns `true`. 
     *       Partially initialized pipe handles are cleaned up automatically
     *       using the RAII `WinHandle` wrapper.
     *
     * @return `true` if the accept operation was successfully posted or `_PipeHandle` is
     *         already valid.
     * @return `false` if the listener has been stopped or a recoverable/transient error occurred,
     *         indicating the operation should be retried later.
     *
     * @throws `std::runtime_error` If a non-recoverable Win32 error occurs during pipe
     *         creation, IOCP registration, or ConnectNamedPipe.
     */
    bool PipeListener::PostAccept()
    {
        if (
            _State.load(std::memory_order_acquire) != State::Starting &&
            IsRunning()
        ) {
            return false;
        }

        // We already have a listening pipe
        if (_PipeHandle) return true;

        constexpr DWORD kPipeBufferSize = 0;
        constexpr DWORD kDefaultTimeoutMs = 0;
        constexpr LPSECURITY_ATTRIBUTES kSecurityAttributes = nullptr;
        
        // Create named pipe instance
        WinHandle pipeHandle(CreateNamedPipeW(
            _PipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            kPipeBufferSize,
            kPipeBufferSize,
            kDefaultTimeoutMs,
            kSecurityAttributes
        ));

        if (!pipeHandle) {
            DWORD errCode = GetLastError();
            if (Utils::Windows::IsRecoverableError(errCode)) {
                return false;
            }

            throw std::runtime_error(
                "Failed to create named pipe instance: " +
                Utils::Windows::FormatErrorMessage(errCode)
            );
        }

        // Register with IOCP
        constexpr DWORD kNumberOfConcurrentThreads = 0;
        if (
            !CreateIoCompletionPort(
                pipeHandle,
                _IOCP,
                reinterpret_cast<ULONG_PTR>(this),
                kNumberOfConcurrentThreads
            )
        ) {
            DWORD errCode = GetLastError();
            if (Utils::Windows::IsRecoverableError(errCode)) {
                return false;
            }
            
            throw std::runtime_error(
                "Failed to register with CreateIoCompletionPort: " +
                Utils::Windows::FormatErrorMessage(errCode)
            );
        }

        ZeroMemory(&_ConnectOverlap, sizeof(_ConnectOverlap));

        _PipeHandle = std::move(pipeHandle);
        _PendingOps.fetch_add(1, std::memory_order_acq_rel);

        BOOL connected = ConnectNamedPipe(_PipeHandle, &_ConnectOverlap);
        DWORD lastErr = connected ? ERROR_SUCCESS : GetLastError();

        if (connected || lastErr == ERROR_PIPE_CONNECTED) {
            // Completed synchronously — client connected immediately
            constexpr DWORD kNumberOfBytesTransferred = 0;
            if (
                !PostQueuedCompletionStatus(
                    _IOCP,
                    kNumberOfBytesTransferred,
                    reinterpret_cast<ULONG_PTR>(this),
                    &_ConnectOverlap
                )
            ) {
                // Failed to post completion — cleanup and report
                DWORD pqErr = GetLastError();
                _PendingOps.fetch_sub(1, std::memory_order_acq_rel);
                _PipeHandle.Reset();
                
                if (Utils::Windows::IsRecoverableError(pqErr)) {
                    return false;
                }

                throw std::runtime_error(
                    "PostQueuedCompletionStatus failed: " +
                    Utils::Windows::FormatErrorMessage(pqErr)
                );
            }
        } else if (lastErr != ERROR_IO_PENDING) {
            _PendingOps.fetch_sub(1, std::memory_order_acq_rel);
            _PipeHandle.Reset();
            
            if (Utils::Windows::IsRecoverableError(lastErr)) {
                return false;
            }

            throw std::runtime_error(
                "ConnectNamedPipe failed: " +
                Utils::Windows::FormatErrorMessage(lastErr)
            );
        }

        return true;
    }

    void PipeListener::TryPostAccept()
    {
        try {
            bool success = RetryWithBackoff([this]{ return PostAccept(); });

            if (!success) {
                HandleFatalError(GetLastError());
            }
        } catch (...) {
            HandleFatalError(GetLastError());
        }
    }

    void PipeListener::HandleFatalError(DWORD errCode) noexcept
    {
        _ErrorInfo.ErrorCode = errCode;
        Stop();
    }

    PipeListener::PipeListener(const WinHandle &iocp, std::wstring pipeName) :
        _IOCP(iocp),
        _PipeName(std::move(pipeName))
    {
    }

    PipeListener::~PipeListener()
    {
        Stop();
    }

    void PipeListener::HandleIoCompletion(DWORD bytesTransferred, OVERLAPPED* pOverlap, DWORD err)
    {
        if (err == ERROR_SUCCESS || err == ERROR_PIPE_CONNECTED)
        {
            // Promote to session
            _OnAccept(std::move(_PipeHandle)); // Server callback
        } else {
            // Cleanup failed/canceled pipe
            _PipeHandle.Reset();
            // ToDo: Implement logging
        }

        if (IsRunning())
            TryPostAccept();

        // Decrement pending operations
        if (_PendingOps.fetch_sub(1, std::memory_order_acq_rel) != 1) return;

        // Notify Stop()
        std::lock_guard<std::mutex> lock(_PendingOpsMutex);
        _PendingOpsCv.notify_all();

        if (_State.load(std::memory_order_acquire) != State::Stopping) return;

        _State.store(State::Stopped, std::memory_order_release);

        if (_OnStop) _OnStop(this);
    }

    template<typename Func>
    requires std::invocable<Func> &&
        std::same_as<std::invoke_result_t<Func>, bool>
    bool PipeListener::RetryWithBackoff(
        Func&& operation,
        uint32_t initialDelayMs,
        uint32_t maxDelayMs,
        uint32_t maxAttempts
    )
    {
        uint32_t delayMs = initialDelayMs;

        for (uint32_t attempt = 0; attempt < maxAttempts; ++attempt) {
            if (std::invoke(std::forward<Func>(operation))) return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            delayMs = std::min(maxDelayMs, delayMs << 2); // exponential backoff capped by maxDelayMs
        }

        return false;
    }

    void PipeListener::Listen()
    {
        if (!_OnAccept) {
            throw std::runtime_error("OnAccept callback must be set before calling Listen()");
        }

        State expected = State::Stopped;
        if (
            !_State.compare_exchange_strong(
                expected,
                State::Starting,
                std::memory_order_acq_rel
            )
        ) {
            // Already started or in invalid state
            return;
        }

        TryPostAccept();

        _State.store(State::Running, std::memory_order_release);
    }

    void PipeListener::Stop() noexcept
    {
        auto currentState = _State.load(std::memory_order_acquire);
        if (
            currentState == State::Stopping ||
            currentState == State::Stopped
        ) {
            // Already stopping or stopped
            return;
        }

        _State.store(State::Stopping, std::memory_order_release);

        if (_PipeHandle) {
            CancelIoEx(_PipeHandle, nullptr);
        }

        try {
            std::unique_lock<std::mutex> lock(_PendingOpsMutex);

            _PendingOpsCv.wait(// ToDo: Implement a timeout
                lock,
                [this]{ return _PendingOps.load(std::memory_order_acquire) == 0; }
            );
        } catch (...) {
            // ToDo: Implement logging
        }

        _PipeHandle.Reset();

        if (_PendingOps.load(std::memory_order_acquire)) {
            // Open async io work, state will be finalized by worker
            return;
        }

        _State.store(State::Stopped, std::memory_order_release);

        if (_OnStop) {
            _OnStop(this);
        }
    }
}
