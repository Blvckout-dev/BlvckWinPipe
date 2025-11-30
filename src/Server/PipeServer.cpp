#include "BlvckWinPipe/Server/PipeServer.h"

#include "BlvckWinPipe/Platform/Platform.h"

namespace Blvckout::BlvckWinPipe::Server
{
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