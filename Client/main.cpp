#include "Client.h"

int main()
{
	raylib::Window window;
	window.SetTargetFPS(60);

	raylib::Camera3D cam(raylib::Vector3(0, 0, 100), raylib::Vector3(0, 0, 0), raylib::Vector3(0, 1, 0), 80, CAMERA_ORTHOGRAPHIC);
	

	Client client;
	client.attemptToConnectToServer("127.0.0.1", 5456);

	while (!window.ShouldClose())
	{
		client.loop();

		BeginDrawing();
		ClearBackground(RAYWHITE);
		BeginMode3D(cam);
		client.draw();
		EndMode3D();
		DrawFPS(10, 10);
		EndDrawing();
	}
	
	return 0;
}