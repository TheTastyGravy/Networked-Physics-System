#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include <vector>
#include <unordered_map>


class Client
{
public:
	Client();
	virtual ~Client();


	// Try to connect to a server
	// Not part of the system, so remove later
	void attemptToConnectToServer(const char* ip, short port);



protected:
	//		-----   THIS CLASS IS INHERITED FROM, SO PROTECTED FUNCTIONS ARE USED INSTEAD OF PUBLIC   -----

	//temp
	void draw()
	{
		BeginDrawing();
		for (auto& it : gameObjects)
		{
			//draw all objects as a red sphere
			DrawCircle3D(it.second->position, 15, { 0,1,0 }, 0, RED);
		}
		EndDrawing();
	}



	// Perform a physics step on objects with prediction. calls getInput
	void physicsUpdate(float deltaTime);

	// Processes the packet if it is used by the system
	void processSystemMessage(const RakNet::Packet* packet);


	// Abstract
	// User defined function for getting player input to send to the server
	virtual void getInput(RakNet::BitStream& bsInOut);

	
	// Abstract
	// User defined factory method to instantiate custom game objects (including client objects)
	//maybe change from bitstream to char* with int size?
	virtual GameObject* gameObjectFactory(unsigned int typeID, RakNet::BitStream& bsIn);

	// Abstract
	virtual StaticObject* staticObjectFactory(unsigned int typeID, RakNet::BitStream& bsIn);

	//abstract function for user to create client object. used by createClientObject

private:
	// Create static obnjects instances from data
	void createStaticObjects(RakNet::BitStream& bsIn);
	// Create a game object instance from data
	void createGameObject(RakNet::BitStream& bsIn);
	// Create the client object we own from data
	void createClientObject(RakNet::BitStream& bsIn);

	// Destroy staticObjects, gameObjects, and myClientObject
	void destroyObjects();




private:
	// The ID assigned to this client by the server
	// The user should not be able to change this, so give them a getter
	unsigned int clientID;
protected:
	unsigned int getClientID() const { return clientID; }


protected:
	RakNet::RakPeerInterface* peerInterface;

	std::vector<StaticObject*> staticObjects;
	//<object ID, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	// The object owned by this client
	ClientObject* myClientObject;
};