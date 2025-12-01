#pragma once

namespace Blvckout::BlvckWinPipe::Server::Pipes
{
    class PipeListener
    {
    private:

    public:
        PipeListener();

        PipeListener(const PipeListener&) = delete;
        PipeListener& operator=(const PipeListener&) = delete;

        PipeListener(PipeListener&&) = delete;
        PipeListener& operator=(PipeListener&&) = delete;

        ~PipeListener();
    };
}
