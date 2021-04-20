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
	virtual GameObject* gameObjectFactory(unsigned int typeID, const RakNet::BitStream& bsIn);

	// Abstract
	virtual StaticObject* staticObjectFactory(unsigned int typeID, const RakNet::BitStream& bsIn);

	//abstract function for user to create client object. used by createClientObject

private:
	// Create static obnjects instances from data
	void createStaticObjects(const RakNet::BitStream& bsIn);
	// Create a game object instance from data
	void createGameObject(const RakNet::BitStream& bsIn);
	// Create the client object we own from data
	void createClientObject(const RakNet::BitStream& bsIn);

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