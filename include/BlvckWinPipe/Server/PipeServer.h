#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "BlvckWinPipe/Export.h"
#include "BlvckWinPipe/Utils/WinHandle.h"
#include "BlvckWinPipe/Server/Pipes/PipeListener.h"

namespace Blvckout::BlvckWinPipe::Server
{
    using Utils::Windows::WinHandle;

    class BLVCKWINPIPE_API PipeServer
    {
    private:
        inline static constexpr wchar_t PIPE_NAME_PREFIX[] = L"\\\\.\\pipe\\";
        std::wstring _Name;
        std::wstring _PipeName;

        WinHandle _IOCP;

        size_t _MaxWorkerThreads;
        std::vector<std::thread> _Workers;

        size_t _MaxListeners { 2u };
        std::vector<std::unique_ptr<Pipes::PipeListener>> _Listeners;

        std::atomic<bool> _IsRunning { false };

        void WorkerThread();

        void OnClientConnect(WinHandle pipeHandle);
    public:
        PipeServer(const std::wstring& name);
        ~PipeServer();

        const std::wstring& Name() const { return _Name; }
        void Name(const std::wstring& name) { 
            _Name = name;
            _PipeName = std::wstring(PIPE_NAME_PREFIX) + _Name;
        }

        std::wstring PipeName() const { return _PipeName; }

        void Start();
        void Stop() noexcept;
    };
} // namespace Blvckout::BlvckWinPipe::Server