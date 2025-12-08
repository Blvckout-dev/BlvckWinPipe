#include "BlvckWinPipe/Server/PipeServer.h"

#include <stdexcept>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Utils/WinUtils.h"

#include "BlvckWinPipe/Server/Pipes/IPipeIoEntity.h"

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

            DWORD errCode = ERROR_SUCCESS;
            if (!ok) {
                errCode = GetLastError();
                // ToDo: Implement logging
            }

            auto pipeEntity = reinterpret_cast<Pipes::IPipeIoEntity*>(completionKey);
            if (!pipeEntity) {
                // ToDo: Implement logging
                continue;
            }

            pipeEntity->HandleIoCompletion(bytesTransferred, lpOverlapped, errCode);
        }
    }

    void PipeServer::OnClientConnect(WinHandle pipeHandle)
    {
        // ToDo: Pass handle to PipeSession and add to sessions vector
    }

    void PipeServer::OnListenerError(Pipes::PipeListener& listener, std::string_view errMsg)
    {
        // ToDo: Restart or recreate listener
        // ToDo: Implement logging
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
        Stop();
    }

    void PipeServer::Start()
    {
        if (_IsRunning.exchange(true, std::memory_order_acq_rel)) return;

        _IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

        if (!_IOCP) {
            DWORD errCode = GetLastError();
            std::string errMsg = "[PipeServer] Failed to create IOCP: " +
                Utils::Windows::FormatErrorMessage(errCode);

            // ToDo: Implement logging
            throw std::runtime_error(errMsg);
        }

        // Spawn worker threads
        for (size_t i = 0; i < _MaxWorkerThreads; ++i) {
            _Workers.emplace_back([this]{ this->WorkerThread(); });
        }

        // Start listeners
        for (size_t i = 0; i < _MaxListeners; i++) {
            auto listener = std::make_unique<Pipes::PipeListener>(_IOCP, PipeName());
            
            listener->SetOnAccept([this](WinHandle pipeHandle) {
                OnClientConnect(std::move(pipeHandle));
            });

            listener->SetOnError([this](Pipes::PipeListener& pipeListener, std::string_view message) {
                OnListenerError(pipeListener, message);
            });
            
            listener->Listen();

            _Listeners.push_back(std::move(listener));
        }
    }

    void PipeServer::Stop() noexcept
    {
        if (!_IsRunning.exchange(false, std::memory_order_acq_rel)) return;

        // Stop listeners
        for (auto &&l : _Listeners) {
            l->Stop();
        }
        _Listeners.clear();

        // Signal workers to exit
        for (size_t i = 0; i < _Workers.size(); ++i) {
            PostQueuedCompletionStatus(_IOCP, 0, 0, nullptr);
        }

        for (auto &t : _Workers) {
            if (t.joinable()) {
                t.join();
            }
        }
        _Workers.clear();
    }
} // namespace Blvckout::BlvckWinPipe::Server