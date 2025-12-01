#pragma once

#include <string>

#include "BlvckWinPipe/Platform/Platform.h"

namespace Blvckout::BlvckWinPipe::Utils::Windows
{
    /**
     * @brief Converts a Windows error code to a human-readable string.
     * 
     * This function wraps the Windows API FormatMessageA to retrieve
     * the textual description of a given error code. If the error code
     * cannot be resolved, it returns a string indicating "Unknown error"
     * along with the numeric code.
     * 
     * Trailing carriage return and line feed characters (\r\n) are trimmed
     * from the returned message.
     * 
     * @param errorCode The Windows error code (DWORD) to format.
     * @return A std::string in the format "<errorCode> - <message>".
     *         For example: "5 - Access is denied."
     *         If the message cannot be retrieved, returns "<errorCode> - Unknown error".
     * 
     * @note This function uses the ANSI version (FormatMessageA) of the Windows API.
     *       For Unicode support, consider using FormatMessageW and converting to UTF-8.
     */
    inline std::string FormatErrorMessage(DWORD errorCode)
    {
        LPSTR buffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr
        );

        std::string code = std::to_string(errorCode);

        if (size == 0 || buffer == nullptr) {
            return code + " - Unknown error";
        }

        std::string message(buffer, size);
        LocalFree(buffer);

        // Trim trailing CR/LF characters
        while (
            !message.empty() &&
            (message.back() == '\r' || message.back() == '\n')
        ) {
            message.pop_back();
        }
        
        return code + " - " + message;
    }
} // namespace Blvckout::BlvckWinPipe::Utils::Windows
