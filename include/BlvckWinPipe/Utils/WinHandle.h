#pragma once

#include "BlvckWinPipe/Platform/Platform.h"

namespace Blvckout::BlvckWinPipe::Utils::Windows
{
    class WinHandle
    {
    private:
        HANDLE _Handle { INVALID_HANDLE_VALUE };

    public:
        WinHandle() = default;
        explicit WinHandle(HANDLE handle) : _Handle(handle) {}
        ~WinHandle() noexcept { Reset(); }

        // Non-copyable
        WinHandle(const WinHandle&) = delete;
        WinHandle& operator=(const WinHandle&) = delete;

        WinHandle(WinHandle&& other) noexcept : _Handle(other._Handle) { other._Handle = INVALID_HANDLE_VALUE; }
        WinHandle& operator=(WinHandle&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                _Handle = other._Handle;
                other._Handle = INVALID_HANDLE_VALUE;
            }
            return *this;
        }

        operator HANDLE() const noexcept { return _Handle; }
        explicit operator bool() const noexcept { return Valid(); }
        WinHandle& operator=(HANDLE handle) noexcept
        {
            Reset(handle);
            return *this;
        }

        bool Valid() const noexcept { return _Handle != INVALID_HANDLE_VALUE && _Handle != nullptr; }

        void Reset(HANDLE newHandle = INVALID_HANDLE_VALUE) noexcept
        {
            if (_Handle != INVALID_HANDLE_VALUE && _Handle != nullptr) {
                CloseHandle(_Handle);
            }
            _Handle = newHandle;
        }
    };
}