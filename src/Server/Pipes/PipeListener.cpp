#include "BlvckWinPipe/Server/Pipes/PipeListener.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    PipeListener::PipeListener(const WinHandle &iocp, std::wstring pipeName) :
        _IOCP(iocp),
        _PipeName(std::move(pipeName))
    {
    }

    PipeListener::~PipeListener()
    {
    }
}
