#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include <vector>
#include <unordered_map>
#include <GetTime.h>
#include "../Shared/Sphere.h"
#include "../Shared/OBB.h"

#include <iostream>


class Server
{
public:
	Server();
	virtual ~Server();


	// Destroy a game object or client object, updating clients
	void destroyObject(unsigned int objectID);
	// Initilize a GameObject. Creation time can be used for objects created in responce to player input
	// Uses gameObjectFactory to initilize user defined types
	void createObject(unsigned int typeID, const PhysicsState& state, const RakNet::Time& creationTime, RakNet::BitStream* customParamiters = nullptr);


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

		// Used to create artificial packet loss and latency
		//peerInterface->ApplyNetworkSimulator(0.1f, 100, 50);

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



		//OBB
		gameObjects[nextObjectID] = new GameObject(PhysicsState({ -30, 10, 0 }, { PI*.0f, PI*.0f, PI*.0f }, { 0,0,0 }, { 0,0,0 }), nextObjectID, 1, 0, new OBB({ 4,4,4 }), 0.7f, 0.5f);
		nextObjectID++;
		//OBB
		//gameObjects[nextObjectID] = new GameObject(PhysicsState({ -30, 20, 0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }), nextObjectID, 1, 0, new OBB({ 4,4,4 }), 0.7f, 0.3f);
		//nextObjectID++;
		//sphere
		//gameObjects[nextObjectID] = new GameObject(PhysicsState({ 30, 10, 0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }), nextObjectID, 1, 1, new Sphere(4), 0.7f, 0.3f);
		//nextObjectID++;

		staticObjects.push_back(new StaticObject({ 0,-30,0 }, { 0,0,0 }, new OBB({ 100,2,100 })));
	}

	void loop()
	{
		RakNet::Packet* packet = nullptr;

		for (packet = peerInterface->Receive(); packet; peerInterface->DeallocatePacket(packet),
			packet = peerInterface->Receive())
		{
			processSystemMessage(packet);
		}

		systemUpdate();
	}


protected:

	//update for the system. collision, object updates, and sending messages
	void systemUpdate();

	// Processes the packet if it is used by the system
	void processSystemMessage(const RakNet::Packet* packet);



	// Abstract
	// User defined factory method for creating game objects with a collider
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, const PhysicsState& state, RakNet::BitStream& bsIn)
	{
		return new GameObject(state, objectID, 1, 1, new OBB({ 4,4,4 }));
	}
	// Abstract
	// User defined factory method for creating client objects when a new client joins
	virtual ClientObject* clientObjectFactory(unsigned int clientID)
	{
		return new ClientObject({ 0,0,0 }, { 0,0,0 }, clientID, 1, 1, new OBB({ 4,4,4 }));
	};

private:
	// Check for collisions and resolve them
	void collisionDetectionAndResolution();


	// Used to send data to a new client including client ID, static objects, game objects, and thier client object
	void onClientConnect(const RakNet::SystemAddress& connectedAddress);
	// Used when a client disconnects. Sends messgae to all other clients to destroy their client object, and removes it from the server
	void onClientDisconnect(const RakNet::SystemAddress& disconnectedAddress);

	// Process player input
	void processInput(unsigned int clientID, RakNet::BitStream& bsIn, const RakNet::Time& timeStamp);


	// Broadcast a message containing the game objects physics state. (Does not use serialize)
	void sendGameObjectUpdate(GameObject* object, RakNet::Time timeStamp);



protected:
	RakNet::RakPeerInterface* peerInterface;
	// Time in milliseconds. Multiply by 0.001 for seconds
	RakNet::Time lastUpdateTime;
	

	// Objects used for static geometry. Need to be created on startup
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