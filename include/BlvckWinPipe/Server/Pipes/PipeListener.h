#pragma once

#include <atomic>
#include <string>

#include "BlvckWinPipe/Utils/WinHandle.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    using Utils::Windows::WinHandle;

    class PipeListener
    {
    private:
        const WinHandle& _IOCP;
        std::wstring _PipeName;

        std::atomic<bool> _IsRunning { false };
    public:
        PipeListener(const WinHandle& iocp, std::wstring pipeName);

        PipeListener(const PipeListener&) = delete;
        PipeListener& operator=(const PipeListener&) = delete;

        PipeListener(PipeListener&&) = delete;
        PipeListener& operator=(PipeListener&&) = delete;

        ~PipeListener();

        void Listen();
        void Stop();
    };
}
