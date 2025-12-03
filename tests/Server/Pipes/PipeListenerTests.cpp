#include <catch2/catch_all.hpp>

#include <future>
#include <thread>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Server/Pipes/PipeListener.h"
#include "PipeListenerTestAccess.h"

using namespace Blvckout::BlvckWinPipe::Server::Pipes;

struct PipeListenerFixture {
    WinHandle iocp;
    PipeListener listener;

    PipeListenerFixture()
        : iocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1))
        , listener(iocp, L"\\\\.\\pipe\\TestPipe")
    {
        listener.SetOnAccept([](WinHandle) {});
    }
};

/*
============================================================
Lifecycle Tests
============================================================
*/

TEST_CASE_METHOD(
    PipeListenerFixture,
    "PipeListener - Initial state",
    "[PipeListener][Lifecycle]"
) {
    REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);
}

TEST_CASE_METHOD(
    PipeListenerFixture,
    "PipeListener - Listen triggers running and PostAccept",
    "[PipeListener][Lifecycle]"
) {
    listener.Listen();
    REQUIRE(PipeListenerTestAccess::IsRunning(listener));
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) > 0);
}

TEST_CASE_METHOD(
    PipeListenerFixture,
    "PipeListener - Stop resets running and cancels pending operations",
    "[PipeListener][Lifecycle]"
) {
    listener.Listen();
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1);

    // Ensure Stop thread is waiting
    std::promise<void> stopStarted;
    auto stopFuture = stopStarted.get_future();

    std::thread stopThread([&]() {
        stopStarted.set_value();
        listener.Stop(); // blocks until pending ops reach zero
    });

    stopFuture.wait();

    // Simulate completion abort to unblock Stop()
    PipeListenerTestAccess::SimulateOperationAbortedCompletion(listener);

    stopThread.join();

    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);
    REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
}

/*
============================================================
Idempotency Tests
============================================================
*/

TEST_CASE_METHOD(
    PipeListenerFixture,
    "PipeListener - Listen is idempotent",
    "[PipeListener][Idempotent]"
) {
    listener.Listen();
    listener.Listen();

    REQUIRE(PipeListenerTestAccess::IsRunning(listener));
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1);
}

TEST_CASE_METHOD(
    PipeListenerFixture,
    "PipeListener - Stop is idempotent",
    "[PipeListener][Idempotent]"
) {
    listener.Stop();
    listener.Stop();

    REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);
}

TEST_CASE_METHOD(
    PipeListenerFixture,
    "PipeListener - PostAccept is idempotent",
    "[PipeListener][Idempotent]"
) {
    // While not running → PostAccept does nothing
    PipeListenerTestAccess::CallPostAccept(listener);
    PipeListenerTestAccess::CallPostAccept(listener);

    REQUIRE(!PipeListenerTestAccess::IsRunning(listener));
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 0);

    // Now enable PostAccept behavior
    listener.Listen();

    PipeListenerTestAccess::CallPostAccept(listener);
    PipeListenerTestAccess::CallPostAccept(listener);

    REQUIRE(PipeListenerTestAccess::IsRunning(listener));
    REQUIRE(PipeListenerTestAccess::PendingOps(listener) == 1);
}