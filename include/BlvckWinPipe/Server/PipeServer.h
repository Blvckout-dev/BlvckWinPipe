#pragma once

#include <string>

#include "BlvckWinPipe/Export.h"

namespace Blvckout::BlvckWinPipe::Server
{
    class BLVCKWINPIPE_API PipeServer
    {
    private:
        inline static constexpr wchar_t PIPE_NAME_PREFIX[] = L"\\\\.\\pipe\\";
        std::wstring _Name;
        std::wstring _PipeName;
    public:
        PipeServer(const std::wstring& name);
        ~PipeServer();

        const std::wstring& Name() const { return _Name; }
        void Name(const std::wstring& name) { 
            _Name = name;
            _PipeName = std::wstring(PIPE_NAME_PREFIX) + _Name;
        }

        std::wstring PipeName() const { return _PipeName; }
    };
} // namespace Blvckout::BlvckWinPipe::Server