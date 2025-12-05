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

    /*!
     * @brief Checks whether a Win32 error code indicates a recoverable error.
     *
     * Errors considered recoverable are typically caused by temporary resource exhaustion
     * or transient unavailability of system objects. These errors are safe to retry.
     *
     * Included recoverable errors:
     * - ERROR_NOT_ENOUGH_MEMORY (8)
     * - ERROR_OUTOFMEMORY (14)
     * - ERROR_NO_SYSTEM_RESOURCES (1450)
     * - ERROR_TOO_MANY_OPEN_FILES (4)
     * - ERROR_PIPE_BUSY (231)
     * - ERROR_BUSY (170)
     *
     * @param errorCode The Win32 error code to check (from GetLastError()).
     * @return `true` if the error code is recoverable/retryable.
     *
     * @note Intended for Win32 APIs where temporary resource exhaustion may occur,
     * such as CreateNamedPipeW, ConnectNamedPipe, or CreateIoCompletionPort.
     */
    inline bool IsRecoverableError(DWORD errorCode) {
        switch (errorCode) {
            // Memory / kernel resources
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY:
            case ERROR_NO_SYSTEM_RESOURCES:

            // Handle / file limits
            case ERROR_TOO_MANY_OPEN_FILES:

            // Pipe busy / object busy
            case ERROR_PIPE_BUSY:
            case ERROR_BUSY:
                return true;

            default:
                return false;
        }
    }
} // namespace Blvckout::BlvckWinPipe::Utils::Windows
