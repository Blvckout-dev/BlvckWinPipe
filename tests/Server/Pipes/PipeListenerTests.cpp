#include <catch2/catch_all.hpp>

#include <future>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Server/Pipes/PipeListener.h"
#include "PipeListenerTestAccess.h"

using namespace Blvckout::BlvckWinPipe::Server::Pipes;

TEST_CASE("PipeListener - Lifecycle", "[PipeListener]") {
    WinHandle iocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1));
    PipeListener listener(iocp, L"\\\\.\\pipe\\TestPipe");

    SECTION("Initial state") {
        REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);
    }

    SECTION("Listen triggers running and PostAccept") {
        listener.Listen(); // automatically triggers PostAccept
        REQUIRE(PipeListenerTestAccess::IsRunning(listener));
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) > 0);
    }

    SECTION("Stop resets running and cancels pending operations") {
        listener.Listen();
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1);

        // Ensure Stop thread waits for pending ops
        std::promise<void> stopStarted;
        std::future<void> stopFuture = stopStarted.get_future();

        std::thread stopThread([&listener, &stopStarted]() {
            stopStarted.set_value();
            listener.Stop(); // blocks until pending ops zero
        });

        stopFuture.wait(); // Ensure Stop is now waiting
        PipeListenerTestAccess::SimulateOperationAbortedCompletion(listener);

        stopThread.join();

        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);
        REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
    }
}

TEST_CASE("PipeListener - Idempotentcy", "[PipeListener]") {
    WinHandle iocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1));
    PipeListener listener(iocp, L"\\\\.\\pipe\\TestPipe");

    SECTION("Listen is idempotent") {
        // First call starts the listener and adds one pending operation
        listener.Listen();
        // Second call should have no effect
        listener.Listen();
        REQUIRE(PipeListenerTestAccess::IsRunning(listener)); // listener must be running
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1); // only one pending operation
    }

    SECTION("Stop is idempotent") {
        // First call stops the listener
        listener.Stop();
        // Second call should have no effect
        listener.Stop();
        REQUIRE(!PipeListenerTestAccess::IsRunning(listener)); // listener must be stopped
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0); // no pending operations
    }

    SECTION("PostAccept is idempotent") {
        // Calling PostAccept while listener is not running should do nothing
        PipeListenerTestAccess::CallPostAccept(listener);
        PipeListenerTestAccess::CallPostAccept(listener);
        REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);

        // Start listener to enable PostAccept behavior
        listener.Listen();
        
        // First PostAccept should add one pending operation
        PipeListenerTestAccess::CallPostAccept(listener);
        // Second PostAccept should not add another pending operation
        PipeListenerTestAccess::CallPostAccept(listener);
        REQUIRE(PipeListenerTestAccess::IsRunning(listener));
        REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1);
    }
}