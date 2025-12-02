#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Utils/WinHandle.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    using Utils::Windows::WinHandle;

    class PipeListener
    {
    private:
        const WinHandle& _IOCP;
        std::wstring _PipeName;

        WinHandle _PipeHandle;
        OVERLAPPED _ConnectOverlap {};

        std::atomic<size_t> _PendingOps {0};
        std::mutex _PendingOpsMutex;
        std::condition_variable _PendingOpsCv;

        std::atomic<bool> _IsRunning { false };

        bool PostAccept();

    public:
        PipeListener(const WinHandle& iocp, std::wstring pipeName);

        PipeListener(const PipeListener&) = delete;
        PipeListener& operator=(const PipeListener&) = delete;

        PipeListener(PipeListener&&) = delete;
        PipeListener& operator=(PipeListener&&) = delete;
        
        ~PipeListener();

        void HandleIoCompletion(DWORD bytesTransferred, OVERLAPPED* pOverlap, DWORD err);

        void Listen();
        void Stop();
    };
}
