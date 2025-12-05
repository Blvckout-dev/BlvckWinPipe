#include "BlvckWinPipe/Server/Pipes/PipeListener.h"

#include <cassert>
#include <stdexcept>

#include "BlvckWinPipe/Utils/WinUtils.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
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
        } else{
            // Cleanup failed/canceled pipe
            _PipeHandle.Reset();
            // ToDo: Implement logging
        }

        try {
            PostAccept();
        } catch(const std::exception& e) {
            // ToDo: Implement logging
        }

        // Decrement pending operations
        if (_PendingOps.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // Notify Stop()
            std::lock_guard<std::mutex> lock(_PendingOpsMutex);
            _PendingOpsCv.notify_all();
        }
    }

    void PipeListener::Listen()
    {
        if (_IsRunning.exchange(true, std::memory_order_acq_rel)) return;

        if (!_OnAccept) {
            throw std::runtime_error("OnAccept callback must be set before calling Listen()");
        }

        if (!PostAccept()) {
            Stop();
            // ToDo: Implement OnError event
        }
    }

    void PipeListener::Stop() noexcept
    {
        if (!_IsRunning.exchange(false, std::memory_order_acq_rel)) return;

        CancelIoEx(_PipeHandle, nullptr);

        try {
            std::unique_lock<std::mutex> lock(_PendingOpsMutex);

            _PendingOpsCv.wait(
                lock,
                [this] { return _PendingOps.load(std::memory_order_acquire) == 0; }
            );
        } catch (...) {
            // ToDo: Implement logging
        }

        _PipeHandle.Reset();
    }
}
