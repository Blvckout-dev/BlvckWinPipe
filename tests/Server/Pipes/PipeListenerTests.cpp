#include <catch2/catch_test_macros.hpp>

#include <future>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Server/Pipes/PipeListener.h"
#include "PipeListenerTestAccess.h"

using namespace Blvckout::BlvckWinPipe::Server::Pipes;

TEST_CASE("PipeListener - Lifecycle", "[PipeListener]") {
    WinHandle iocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1));
    PipeListener listener(iocp, L"\\\\.\\pipe\\TestPipe");

    SECTION("Init - Not running") {
        REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
    }

    SECTION("Listen - Sets running") {
        listener.Listen();
        REQUIRE(PipeListenerTestAccess::IsRunning(listener));
    }

    SECTION("Stop - Resets running") {
        listener.Listen();
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1);

        // Promise to ensure Stop thread is waiting
        std::promise<void> stopStarted;
        std::future<void> stopFuture = stopStarted.get_future();

        // Thread that simulates listener stop
        std::thread listenerStopThread([&listener, &stopStarted]() {
            stopStarted.set_value();
            listener.Stop(); // blocks until pending ops zero
        });

        stopFuture.wait(); // Ensure Stop is now waiting
        PipeListenerTestAccess::SimulateOperationAbortedCompletion(listener);

        listenerStopThread.join();

        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);
        REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
    }
}