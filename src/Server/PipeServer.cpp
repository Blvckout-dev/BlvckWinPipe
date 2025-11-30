#include "BlvckWinPipe/Server/PipeServer.h"

namespace Blvckout::BlvckWinPipe::Server
{
    PipeServer::PipeServer(const std::wstring& name) :
        _Name(name),
        _PipeName(std::wstring(PIPE_NAME_PREFIX) + name)
    {
    }

    PipeServer::~PipeServer()
    {
    }
} // namespace Blvckout::BlvckWinPipe::Server