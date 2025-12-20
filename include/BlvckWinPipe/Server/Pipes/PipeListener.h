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
        using StopCallback = std::function<void(PipeListener*)>;

        enum class State : std::uint8_t {
            Stopped,
            Stopping,
            Starting,
            Running
        };

        struct ErrorInfo {
            DWORD ErrorCode { ERROR_SUCCESS };

            bool HasError() const noexcept {
                return ErrorCode != ERROR_SUCCESS;
            }
        };

    private:
        const WinHandle& _IOCP;
        std::wstring _PipeName;

        WinHandle _PipeHandle;
        OVERLAPPED _ConnectOverlap {};

        std::atomic<size_t> _PendingOps {0};
        std::mutex _PendingOpsMutex;
        std::condition_variable _PendingOpsCv;

        std::atomic<State> _State { State::Stopped };

        ErrorInfo _ErrorInfo;

        bool PostAccept();
        void TryPostAccept();
        
        template<typename Func>
        requires std::invocable<Func> &&
            std::same_as<std::invoke_result_t<Func>, bool>
        bool RetryWithBackoff(
            Func&& operation,
            uint32_t initialDelayMs = 5u,
            uint32_t maxDelayMs = 1000u,
            uint32_t maxAttempts = 5u
        );

        void HandleFatalError(DWORD errCode) noexcept;

        // Events
        AcceptCallback _OnAccept;
        StopCallback _OnStop;

    public:
        PipeListener(const WinHandle& iocp, std::wstring pipeName);

        PipeListener(const PipeListener&) = delete;
        PipeListener& operator=(const PipeListener&) = delete;

        PipeListener(PipeListener&&) = delete;
        PipeListener& operator=(PipeListener&&) = delete;
        
        ~PipeListener();

        void HandleIoCompletion(DWORD bytesTransferred, OVERLAPPED* pOverlap, DWORD err);

        void Listen();
        void Stop() noexcept;

        bool IsRunning() const noexcept {
            return _State.load(std::memory_order_acquire) == State::Running;
        }

        const ErrorInfo& GetErrorInfo() const noexcept { return _ErrorInfo; };

        // Events
        void SetOnAccept(AcceptCallback cb) { _OnAccept = std::move(cb); }
        void SetOnStop(StopCallback cb) { _OnStop = std::move(cb); }

        // Testing
        friend class PipeListenerTestAccess;
    };
}
