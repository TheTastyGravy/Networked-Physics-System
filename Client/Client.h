#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include <vector>
#include <unordered_map>
#include "../Shared/RingBuffer.h"


class Client
{
public:
	Client();
	virtual ~Client();


	// Try to connect to a server
	// Not part of the system, so remove later
	void attemptToConnectToServer(const char* ip, short port);


	//temp
	void draw()
	{
		for (auto& it : gameObjects)
		{
			DrawSphere(it.second->position, 4, RED);
		}
		if (myClientObject)
		{
			DrawSphere(myClientObject->position, 4, BLACK);
		}
	}

	void loop()
	{
		RakNet::Packet* packet = nullptr;

		for (packet = peerInterface->Receive(); packet; peerInterface->DeallocatePacket(packet),
			packet = peerInterface->Receive())
		{
			processSystemMessage(packet);
		}

		physicsUpdate();
	}


protected:
	//		-----   THIS CLASS IS INHERITED FROM, SO PROTECTED FUNCTIONS ARE USED INSTEAD OF PUBLIC   -----


	// Perform a physics step on objects with prediction. calls getInput
	void physicsUpdate();

	// Processes the packet if it is used by the system
	void processSystemMessage(const RakNet::Packet* packet);


	// Abstract
	// User defined function for getting player input to send to the server
	// Input struct is defined in ClientObject.h
	virtual Input getInput()
	{
		Input input;
		input.wDown = IsKeyDown(KEY_W);
		input.aDown = IsKeyDown(KEY_A);
		input.sDown = IsKeyDown(KEY_S);
		input.dDown = IsKeyDown(KEY_D);

		return input;
	}

	
	// Abstract
	// User defined factory method to create static objects when connecting to a server
	virtual StaticObject* staticObjectFactory(unsigned int typeID, raylib::Vector3 position, raylib::Vector3 rotation, RakNet::BitStream& bsIn)
	{
		return new StaticObject(position, rotation);
	}
	// Abstract
	// User defined factory method to create game objects (including other client objects)
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, PhysicsState state, RakNet::BitStream& bsIn)
	{
		return new GameObject(state.position, state.rotation, objectID, 1);
	}
	// Abstract
	// User defined factory method to create the client object for this Client instance
	virtual ClientObject* clientObjectFactory(unsigned int typeID, PhysicsState state, RakNet::BitStream& bsIn)
	{
		return new ClientObject(state.position, state.rotation, getClientID(), 1);
	}


private:
	// Create static object instances from data
	void createStaticObjects(RakNet::BitStream& bsIn);
	// Create a game object instance from data
	void createGameObject(RakNet::BitStream& bsIn);
	// Create the client object we own from data
	void createClientObject(RakNet::BitStream& bsIn);

	// Destroy the game object of objectID
	void destroyGameObject(unsigned int objectID);
	// Destroy all staticObjects, gameObjects, and myClientObject
	void destroyAllObjects();

	// Used when an object update is receved from the server
	void applyServerUpdate(RakNet::BitStream& bsIn, const RakNet::Time& timeStamp);


private:
	// The ID assigned to this client by the server
	// The user should not be able to change this, so give them a getter
	unsigned int clientID;
protected:
	unsigned int getClientID() const { return clientID; }


protected:
	RakNet::RakPeerInterface* peerInterface;

	// Used to determine delta time
	RakNet::Time lastUpdateTime;

	std::vector<StaticObject*> staticObjects;
	//<object ID, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	// The object owned by this client
	ClientObject* myClientObject;


	// Buffer of player inputs with their time
	RingBuffer<std::pair<RakNet::Time, Input>> inputBuffer;
};