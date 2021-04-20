#pragma once
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



	//abstract game object factory method. unlike client, these need colliders
	
	//abstract function for creating new client object when a client connects. user defined position, rotation, etc


private:
	// Check for collisions and resolve them
	void collisionDetectionAndRolslution();
	// Apply physics steps to game objects
	void applyPhysicsStep(float timeStep);

	// Used to send data to a new client including client ID, static objects, game objects, and thier client object
	void onClientConnect(const RakNet::SystemAddress& connectedAddress);
	// Used when a client disconnects. Sends messgae to all other clients to destroy their client object, and removes it from the server
	void onClientDisconnect(unsigned int clientID);

	// Process player input
	void processInput(const RakNet::BitStream* bsIn);




protected:
	RakNet::RakPeerInterface* peerInterface;


	std::vector<StaticObject*> staticObjects;
	//<object id, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	//<client id, client object>
	std::unordered_map<unsigned int, ClientObject*> clientObjects;

};