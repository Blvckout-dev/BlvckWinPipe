#include <catch2/catch_test_macros.hpp>

#include "BlvckWinPipe/Platform/Platform.h"
#include "BlvckWinPipe/Utils/WinHandle.h"

using Blvckout::BlvckWinPipe::Utils::Windows::WinHandle;

TEST_CASE("WinHandle default constructor", "[WinHandle][RAII]") {
    WinHandle h;
    
    REQUIRE_FALSE(h.Valid());
    REQUIRE(static_cast<HANDLE>(h) == INVALID_HANDLE_VALUE);
}

TEST_CASE("WinHandle construct with valid handle", "[WinHandle][RAII]") {
    HANDLE raw = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    REQUIRE(raw != nullptr);

    WinHandle h(raw);
    
    REQUIRE(h.Valid());
    REQUIRE(static_cast<HANDLE>(h) == raw);
}

TEST_CASE("WinHandle move constructor", "[WinHandle][RAII][MoveSemantics]") {
    HANDLE raw = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    
    WinHandle h1(raw);
    WinHandle h2(std::move(h1));

    REQUIRE(h2.Valid());
    REQUIRE_FALSE(h1.Valid());
}

TEST_CASE("WinHandle move assignment", "[WinHandle][RAII][MoveSemantics]") {
    HANDLE raw1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HANDLE raw2 = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    WinHandle h1(raw1);
    WinHandle h2(raw2);

    h2 = std::move(h1);

    REQUIRE(h2.Valid());
    REQUIRE_FALSE(h1.Valid());
}

TEST_CASE("WinHandle reset to invalid handle", "[WinHandle][RAII]") {
    HANDLE raw = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    WinHandle h(raw);
    h.Reset();
    
    REQUIRE_FALSE(h.Valid());
    REQUIRE(static_cast<HANDLE>(h) == INVALID_HANDLE_VALUE);
}

TEST_CASE("WinHandle reset to new handle", "[WinHandle][RAII][MoveSemantics]") {
    HANDLE raw1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HANDLE raw2 = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    WinHandle h(raw1);
    h.Reset(raw2);

    REQUIRE(h.Valid());
    REQUIRE(static_cast<HANDLE>(h) == raw2);
}