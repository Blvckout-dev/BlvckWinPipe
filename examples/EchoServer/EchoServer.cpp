#include <BlvckWinPipe/Server/PipeServer.h>

using namespace Blvckout::BlvckWinPipe::Server;

int main(int argc, char const *argv[])
{
    PipeServer server(L"EchoServer");
    server.Start();
    
    system("pause");

    server.Stop();

    return 0;
}