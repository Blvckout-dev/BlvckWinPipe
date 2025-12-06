#pragma once

#include <atomic>
#include <concepts>
#include <functional>
#include <mutex>
#include <string>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Utils/WinHandle.h"
#include "BlvckWinPipe/Server/Pipes/IPipeIoEntity.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    using Utils::Windows::WinHandle;

    class PipeListener : IPipeIoEntity
    {
    public:
        using AcceptCallback = std::function<void(WinHandle)>;

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
        
        template<typename Func>
        requires std::invocable<Func> &&
            std::same_as<std::invoke_result_t<Func>, bool>
        bool RetryWithBackoff(
            Func&& operation,
            uint32_t initialDelayMs = 5u,
            uint32_t maxDelayMs = 1000u,
            uint32_t maxAttempts = 5u
        );

        // Events
        AcceptCallback _OnAccept;

    public:
        PipeListener(const WinHandle& iocp, std::wstring pipeName) noexcept;

        PipeListener(const PipeListener&) = delete;
        PipeListener& operator=(const PipeListener&) = delete;

        PipeListener(PipeListener&&) = delete;
        PipeListener& operator=(PipeListener&&) = delete;
        
        ~PipeListener();

        void HandleIoCompletion(DWORD bytesTransferred, OVERLAPPED* pOverlap, DWORD err);

        void Listen();
        void Stop() noexcept;

        // Events
        void SetOnAccept(AcceptCallback cb) { _OnAccept = std::move(cb); }

        // Testing
        friend class PipeListenerTestAccess;
    };
}
