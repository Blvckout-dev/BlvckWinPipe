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

    void PipeListener::Listen()
    {
        if (_IsRunning.exchange(true, std::memory_order_acq_rel)) return;
    }

    void PipeListener::Stop()
    {
        if (!_IsRunning.exchange(false, std::memory_order_acq_rel)) return;
    }
}
