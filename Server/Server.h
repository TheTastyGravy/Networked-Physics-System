#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include <vector>
#include <unordered_map>
#include <GetTime.h>

#include <iostream>
#include <thread>
#include <chrono>



class Server
{
public:
	Server();
	virtual ~Server();


	// Destroy a game object or client object, updating clients
	void destroyObject(unsigned int objectID);
	// Initilize a GameObject. Creation time can be used for objects created in responce to player input
	// Uses gameObjectFactory to initilize user defined types
	// ***** add collider info *****
	void createObject(unsigned int typeID, const PhysicsState& state, const RakNet::TimeMS& creationTime, RakNet::BitStream& customParamiters);


	//	----------		TEMP FUNCTIONS FOR DEBUGGING	----------
	void start()
	{
		// Startup the server and start it listening to clients
		std::cout << "Starting up the server..." << std::endl;

		// Create a socket descriptor for this connection
		RakNet::SocketDescriptor sd(5456, 0);

		// Startup the interface with a max of 32 connections
		peerInterface->Startup(32, &sd, 1);
		peerInterface->SetMaximumIncomingConnections(32);
		// Automatic pinging for timestamping
		peerInterface->SetOccasionalPing(true);

		// Output state of server
		if (peerInterface->IsActive())
		{
			std::cout << "Server setup sucessful" << std::endl;
			std::cout << "Server address: " << peerInterface->GetMyBoundAddress().ToString() << std::endl;
		}
		else
		{
			std::cout << "Server failed to setup" << std::endl;
		}


		gameObjects[nextObjectID] = new GameObject(raylib::Vector3(20,20), { 0,0,0 }, nextObjectID, 1);
		gameObjects[nextObjectID]->setVelocity(raylib::Vector3(5, 5));
		nextObjectID++;
	}

	void loop()
	{
		RakNet::Packet* packet = nullptr;

		while (true)
		{
			for (packet = peerInterface->Receive(); packet; peerInterface->DeallocatePacket(packet),
				packet = peerInterface->Receive())
			{
				processSystemMessage(packet);
			}


			raylib::Vector3 v = gameObjects[101]->getVelocity().Normalize();
			raylib::Vector3 perp = v.Perpendicular();

			v += perp * 0.1f;

			gameObjects[101]->setVelocity(v.Normalize() * 10);


			physicsUpdate();

			std::this_thread::sleep_for(std::chrono::milliseconds(33));
		}
	}


protected:

	//update for the system. collision, object updates, and sending messages
	void physicsUpdate();

	// Processes the packet if it is used by the system
	void processSystemMessage(const RakNet::Packet* packet);



	// Abstract
	// User defined factory method to instantiate custom game objects with a collider
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, const PhysicsState& state, RakNet::BitStream& bsIn)
	{
		throw EXCEPTION_ACCESS_VIOLATION;
	}
	
	//abstract function for creating new client object when a client connects. user defined position, rotation, etc
	//this doesnt have a bitstream because it is used in responce to a client joinging, not a message
	virtual ClientObject* clientObjectFactory(unsigned int clientID)
	{
		return new ClientObject({ 0,0,0 }, { 0,0,0 }, clientID, 1);
	};

private:
	// Check for collisions and resolve them
	void collisionDetectionAndResolution();

	// Used to send data to a new client including client ID, static objects, game objects, and thier client object
	void onClientConnect(const RakNet::SystemAddress& connectedAddress);
	// Used when a client disconnects. Sends messgae to all other clients to destroy their client object, and removes it from the server
	void onClientDisconnect(const RakNet::SystemAddress& disconnectedAddress);

	// Process player input
	void processInput(unsigned int clientID, RakNet::BitStream& bsIn);


	// Broadcast a message containing the game objects physics state. (Does not use serialize)
	void sendGameObjectUpdate(GameObject* object);



protected:
	RakNet::RakPeerInterface* peerInterface;
	// Time in milliseconds. Multiply by 0.001 for seconds
	RakNet::TimeMS lastUpdateTime;


	// Static objects are used for constant geometry, and needs to be created before clients join the server
	std::vector<StaticObject*> staticObjects;
	//<object id, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	//<client id, client object>
	std::unordered_map<unsigned int, ClientObject*> clientObjects;

	//<client address, client ID>
	std::unordered_map<unsigned long, unsigned int> addressToClientID;


private:
	// Identifier for a client and the ClientObject they own. ALWAYS INCREMENT AFTER USE
	unsigned int nextClientID = 1;
	// Identifier for a GameObject. ALWAYS INCREMENT AFTER USE
	unsigned int nextObjectID = 101;
};