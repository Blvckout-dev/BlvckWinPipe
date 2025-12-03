#pragma once

#include "BlvckWinPipe/Server/Pipes/PipeListener.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    /**
     * @brief Test-only access to PipeListener internals.
     *
     * Allows unit tests to access internal state, such as lifecycle status
     * (_IsRunning) and pending operations (_PendingOps), without exposing it publicly.
     */
    class PipeListenerTestAccess
    {
    public:
        static bool IsRunning(const PipeListener& listener) noexcept
        {
            return listener._IsRunning.load(std::memory_order_acquire);
        }

        static size_t PendingOps(const PipeListener& listener) noexcept
        {
            return listener._PendingOps.load(std::memory_order_acquire);
        }

        static bool CallPostAccept(PipeListener& listener) {
            return listener.PostAccept();
        }

        static void SimulateOperationAbortedCompletion(PipeListener& listener) {
            listener.HandleIoCompletion(0, nullptr, ERROR_OPERATION_ABORTED);
        }
    };
}