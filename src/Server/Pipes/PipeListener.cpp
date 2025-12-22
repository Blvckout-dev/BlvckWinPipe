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
     * and initiates an overlapped `ConnectNamedPipe` operation. If the operation is posted
     * successfully, the pipe handle is stored in `_PipeHandle` and `_PendingOps` is incremented
     * to track the pending I/O.
     * 
     * @return DWORD Win32 error code:
     * - `ERROR_SUCCESS` if the accept operation was successfully posted or is already pending
     * - `ERROR_OPERATION_ABORTED` if the listener is stopping or has already stopped.
     * - Otherwise: The Win32 error code indicating why the accept could not be posted
     * 
     * @note Completion of the accept operation is reported asynchronously via the IO completion port
     * and does not imply that a client has already connected at the time this function returns.
     * 
     * @note This function is idempotent: if an accept operation is already pending (i.e. `_PipeHandle`
     * is valid), no new operation is posted and `ERROR_SUCCESS` is returned.
     */
    DWORD PipeListener::PostAccept() noexcept
    {
        if (
            _State.load(std::memory_order_acquire) == State::Stopping ||
            _State.load(std::memory_order_acquire) == State::Stopped
        ) {
            return ERROR_OPERATION_ABORTED;
        }

        // We already have a listening pipe
        if (_PipeHandle) return ERROR_SUCCESS;

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
            return GetLastError();
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
            return GetLastError();
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
                
                return pqErr;
            }
        } else if (lastErr != ERROR_IO_PENDING) {
            _PendingOps.fetch_sub(1, std::memory_order_acq_rel);
            _PipeHandle.Reset();
            
            return lastErr;
        }

        return ERROR_SUCCESS;
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

        if (IsRunning()) {
            DWORD lastErr = PostAccept();
            if (lastErr != ERROR_SUCCESS) {
                if (Utils::Windows::IsRecoverableError(lastErr)) {
                    // ToDo: Retry via ThreadPoolTimer
                } else {
                    HandleFatalError(lastErr);
                }
            }
        }

        // Decrement pending operations
        if (_PendingOps.fetch_sub(1, std::memory_order_acq_rel) != 1) return;

        // Notify Stop()
        std::lock_guard<std::mutex> lock(_PendingOpsMutex);
        _PendingOpsCv.notify_all();

        if (_State.load(std::memory_order_acquire) != State::Stopping) return;

        _State.store(State::Stopped, std::memory_order_release);

        if (_OnStop) _OnStop(this);
    }

    bool PipeListener::Listen()
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
            return false;
        }

        DWORD errCode = PostAccept();
        if (errCode != ERROR_SUCCESS) {
            HandleFatalError(errCode);
            return false;
        }

        _State.store(State::Running, std::memory_order_release);
        return true;
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
