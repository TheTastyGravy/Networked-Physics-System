#include "Server.h"
#include <thread>

int main()
{
	Server server;
	server.start();

	std::thread thread(&Server::loop, &server);

	while (true)
	{
	}

	return 0;
}