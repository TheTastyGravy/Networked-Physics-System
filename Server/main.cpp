#include "Server.h"
#include <thread>

int main()
{
	Server server;
	server.start();

	while (true)
	{
		server.loop();
		std::this_thread::sleep_for(std::chrono::milliseconds(33));
	}

	return 0;
}