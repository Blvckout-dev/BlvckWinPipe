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
        if (!_IsRunning.load(std::memory_order_acquire)) return false;

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

        BOOL connected = ConnectNamedPipe(pipeHandle, &_ConnectOverlap);
        _PendingOps.fetch_add(1, std::memory_order_acq_rel);
        
        DWORD lastErr = ERROR_SUCCESS;
        if (!connected) lastErr = GetLastError();

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
                return false;
            }
        } else if (lastErr != ERROR_IO_PENDING) {
            _PendingOps.fetch_sub(1, std::memory_order_acq_rel);
            return false;
        }

        _PipeHandle = std::move(pipeHandle);
        return true;
    }

    void PipeListener::TryPostAccept()
    {
        try {
            bool success = RetryWithBackoff([this]{ return PostAccept(); });

            if (!success && _IsRunning.load(std::memory_order_acquire)) {
                constexpr char kPostAcceptFailedMsg[] =
                    "Failed to post a accept for new connections after retries";
                
                StopAndNotifyError(kPostAcceptFailedMsg);
            }
        } catch (const std::exception& e) {
            StopAndNotifyError(e.what());
        } catch (...) {
            StopAndNotifyError("Unknown error");
        }
    }

    void PipeListener::StopAndNotifyError(std::string_view message)
    {
        Stop();
        _OnError(*this, message);
    }

    PipeListener::PipeListener(const WinHandle &iocp, std::wstring pipeName) noexcept :
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

        TryPostAccept();

        // Decrement pending operations
        if (_PendingOps.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // Notify Stop()
            std::lock_guard<std::mutex> lock(_PendingOpsMutex);
            _PendingOpsCv.notify_all();
        }
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
            if (!_IsRunning.load(std::memory_order_acquire)) return false;
            if (std::invoke(std::forward<Func>(operation))) return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            delayMs = std::min(maxDelayMs, delayMs << 2); // exponential backoff capped by maxDelayMs
        }

        return false;
    }

    void PipeListener::Listen()
    {
        if (!_OnError) {
            throw std::runtime_error("OnError callback must be set before calling Listen()");
        }

        if (!_OnAccept) {
            throw std::runtime_error("OnAccept callback must be set before calling Listen()");
        }

        if (_IsRunning.exchange(true, std::memory_order_acq_rel)) return;

        TryPostAccept();
    }

    void PipeListener::Stop() noexcept
    {
        if (!_IsRunning.exchange(false, std::memory_order_acq_rel)) return;

        CancelIoEx(_PipeHandle, nullptr);

        try {
            std::unique_lock<std::mutex> lock(_PendingOpsMutex);

            _PendingOpsCv.wait(
                lock,
                [this]{ return _PendingOps.load(std::memory_order_acquire) == 0; }
            );
        } catch (...) {
            // ToDo: Implement logging
        }

        _PipeHandle.Reset();
    }
}
