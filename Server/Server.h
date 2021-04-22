#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include <vector>
#include <unordered_map>


class Server
{
public:
	// Destroy a game object or client object, updating clients
	void destroyObject(unsigned int objectID);
	//use a factory method to create game object with next ID, and send it to clients
	void createObject(/*typeID, param data, etc*/);



protected:

	//do all physics stuff. collision detection & resolution, physics step on objects, and sending updates to clients
	void physicsUpdate(float deltaTime);

	// Processes the packet if it is used by the system
	void processSystemMessage(const RakNet::Packet* packet);



	// Abstract
	// User defined factory method to instantiate custom game objects with a collider
	//maybe change from bitstream to char* with int size?
	virtual GameObject* gameObjectFactory(unsigned int typeID, RakNet::BitStream& bsIn)
	{
		return new GameObject(raylib::Vector3(0), raylib::Vector3(0), )
	}
	
	//abstract function for creating new client object when a client connects. user defined position, rotation, etc
	virtual ClientObject* clientObjectFactory(unsigned int clientID);

private:
	// Check for collisions and resolve them
	void collisionDetectionAndResolution();

	// Used to send data to a new client including client ID, static objects, game objects, and thier client object
	void onClientConnect(const RakNet::SystemAddress& connectedAddress);
	// Used when a client disconnects. Sends messgae to all other clients to destroy their client object, and removes it from the server
	void onClientDisconnect(const RakNet::SystemAddress& disconnectedAddress);

	// Process player input
	void processInput(unsigned int clientID, RakNet::BitStream* bsIn);




protected:
	RakNet::RakPeerInterface* peerInterface;
	RakNet::Time currentTime;


	std::vector<StaticObject*> staticObjects;
	//<object id, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	//<client id, client object>
	std::unordered_map<unsigned int, ClientObject*> clientObjects;

	//<client address, client ID>
	std::unordered_map<unsigned long, unsigned int> addressToClientID;


	unsigned int nextClientID = 1;
	unsigned int nextObjectID = 101;
};