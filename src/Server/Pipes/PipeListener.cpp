#include "BlvckWinPipe/Server/Pipes/PipeListener.h"

#include <stdexcept>

#include "BlvckWinPipe/Utils/WinUtils.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    bool PipeListener::PostAccept()
    {
        if (!_IsRunning) return false;

        constexpr DWORD kPipeBufferSize = 0;
        constexpr DWORD kDefaultTimeoutMs = 0;
        constexpr LPSECURITY_ATTRIBUTES kSecurityAttributes = nullptr;
        
        // Create named pipe instance
        _PipeHandle = CreateNamedPipeW(
            _PipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            kPipeBufferSize,
            kPipeBufferSize,
            kDefaultTimeoutMs,
            kSecurityAttributes
        );

        if (!_PipeHandle) {
            DWORD errorCode = GetLastError();
            throw std::runtime_error(
                "Failed to create named pipe instance: " +
                Utils::Windows::FormatErrorMessage(errorCode)
            );
        }

        // Register with IOCP
        constexpr DWORD kNumberOfConcurrentThreads = 0;
        if (
            !CreateIoCompletionPort(
                _PipeHandle,
                _IOCP,
                reinterpret_cast<ULONG_PTR>(this),
                kNumberOfConcurrentThreads
            )
        ) {
            DWORD error = GetLastError();
            _PipeHandle.Reset();
            throw std::runtime_error(
                "Failed to register with CreateIoCompletionPort: " +
                Utils::Windows::FormatErrorMessage(error)
            );
        }

        ZeroMemory(&_ConnectOverlap, sizeof(_ConnectOverlap));

        BOOL connected = ConnectNamedPipe(_PipeHandle, &_ConnectOverlap);
        
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
                _PipeHandle.Reset();
                throw std::runtime_error(
                    "Failed to post queued completion status: " +
                    Utils::Windows::FormatErrorMessage(pqErr)
                );
            }
        } else if (lastErr != ERROR_IO_PENDING) {
            _PipeHandle.Reset();
            return false;
        }

        return true;
    }

    PipeListener::PipeListener(const WinHandle &iocp, std::wstring pipeName) :
        _IOCP(iocp),
        _PipeName(std::move(pipeName))
    {
    }

    PipeListener::~PipeListener()
    {
    }

    void PipeListener::Listen()
    {
        if (_IsRunning.exchange(true, std::memory_order_acq_rel)) return;
    }

    void PipeListener::Stop()
    {
        if (!_IsRunning.exchange(false, std::memory_order_acq_rel)) return;
    }
}
