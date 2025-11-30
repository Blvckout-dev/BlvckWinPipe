#include "BlvckWinPipe/Server/PipeServer.h"

#include "BlvckWinPipe/Platform/Platform.h"

namespace Blvckout::BlvckWinPipe::Server
{
    void PipeServer::WorkerThread()
    {
        while (_IsRunning) {
            DWORD bytesTransferred {};
            ULONG_PTR completionKey {};
            LPOVERLAPPED lpOverlapped { nullptr };
            
            BOOL ok = GetQueuedCompletionStatus(
                _IOCP,
                &bytesTransferred,
                &completionKey,
                &lpOverlapped,
                INFINITE
            );

            // Shutdown signal
            if (completionKey == 0 && lpOverlapped == nullptr) {
                break;
            }
        }
    }

    PipeServer::PipeServer(const std::wstring& name) :
        _Name(name),
        _PipeName(std::wstring(PIPE_NAME_PREFIX) + name),
        _IOCP(INVALID_HANDLE_VALUE),
        _MaxWorkerThreads(
            std::max(std::thread::hardware_concurrency(), 2u)
        )
    {
    }

    PipeServer::~PipeServer()
    {
    }
} // namespace Blvckout::BlvckWinPipe::Server