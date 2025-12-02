#pragma once

#include "BlvckWinPipe/Platform/Platform.h"

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    /**
     * @brief Interface for IO-capable pipe entities used with IOCP.
     *
     * This interface defines a contract for objects that handle asynchronous
     * I/O operations in a PipeServer, such as PipeListener and PipeSession.
     *
     * Implementers must provide a thread-safe implementation of
     * HandleIoCompletion, which will be called by the WorkerThread when an
     * IO operation completes.
     */
    class IPipeIoEntity
    {
    public:
        virtual ~IPipeIoEntity() = default;

        /**
         * @brief Called by the IOCP WorkerThread when an asynchronous IO operation completes.
         *
         * Implementers should handle the completed IO operation, including:
         * - Processing received data
         * - Cleaning up failed or canceled operations
         * - Posting new accept operations if applicable
         *
         * @param bytesTransferred Number of bytes transferred in the IO operation
         * @param pOverlap Pointer to the OVERLAPPED structure associated with this operation
         * @param err Error code returned by the IO operation (ERROR_SUCCESS, ERROR_IO_PENDING, etc.)
         */
        virtual void HandleIoCompletion(DWORD bytesTransferred, OVERLAPPED* pOverlap, DWORD err) = 0;
    };
} // namespace Blvckout::BlvckWinPipe::Server::Pipes