#include "Client.h"
#include "../Shared/GameMessages.h"
#include <BitStream.h>
#include <GetTime.h>
#include <iostream>


Client::Client()
{
	peerInterface = RakNet::RakPeerInterface::GetInstance();
	myClientObject = nullptr;
	lastUpdateTime = RakNet::GetTimeMS();
	clientID = -1;
}

Client::~Client()
{
	peerInterface->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(peerInterface);
	destroyObjects();
}


void Client::attemptToConnectToServer(const char* ip, short port)
{
    if (peerInterface == nullptr)
    {
        peerInterface = RakNet::RakPeerInterface::GetInstance();
    }

	// Close any existing connections
	peerInterface->Shutdown(300);


    // No data is needed since we are connecting
    RakNet::SocketDescriptor sd;
    // 1 max connection since connecting
    peerInterface->Startup(1, &sd, 1);
	// Automatic pinging for timestamping
	peerInterface->SetOccasionalPing(true);

    std::cout << "Connecting to server at: " << ip << std::endl;

    // Attempt to connect to the server
    RakNet::ConnectionAttemptResult res = peerInterface->Connect(ip, port, nullptr, 0);
    if (res != RakNet::CONNECTION_ATTEMPT_STARTED)
    {
        std::cout << "Unable to start connection, error number: " << res << std::endl;
    }
}



void Client::createStaticObjects(RakNet::BitStream& bsIn)
{
	//get each static object instance, and create them using user defined factory method
	//{typeid, data...}
}

void Client::createGameObject(RakNet::BitStream& bsIn)
{
	//use factory method to create new game object and use its object id to map it

	unsigned int id;
	bsIn.Read(id);

	//ignore type id for now
	bsIn.IgnoreBytes(sizeof(unsigned int));

	raylib::Vector3 position;
	raylib::Vector3 rotation;
	bsIn.Read(position);
	bsIn.Read(rotation);
	//vel
	//angular vel

	gameObjects[id] = new GameObject(position, rotation, id, 1);
}

void Client::createClientObject(RakNet::BitStream& bsIn)
{
	//get defined info from packet, then send the rest to user function

	unsigned int id;
	bsIn.Read(id);

	//ignore type id for now
	bsIn.IgnoreBytes(sizeof(unsigned int));

	raylib::Vector3 position;
	raylib::Vector3 rotation;
	bsIn.Read(position);
	bsIn.Read(rotation);

	myClientObject = new ClientObject(position, rotation, id, 1);
}


void Client::destroyObjects()
{
	clientID = -1;

	// Destroy static objects
	for (auto it : staticObjects)
	{
		delete it;
	}
	staticObjects.clear();

	// Destroy game objects
	for (auto& it : gameObjects)
	{
		delete it.second;
	}
	gameObjects.clear();

	// Destroy client object
	if (myClientObject)
	{
		delete myClientObject;
		myClientObject = nullptr;
	}
}


void Client::applyServerUpdate(RakNet::BitStream& bsIn)
{
	// Get time stamp and object ID
	RakNet::TimeMS time;
	bsIn.Read(time);
	unsigned int id;
	bsIn.Read(id);


	// Find the object with the ID
	GameObject* obj = (id == clientID) ? myClientObject : nullptr;
	if (gameObjects.count(id) > 0)
	{
		obj = gameObjects[id];
	}
	// Check if we found the object
	if (obj == nullptr)
	{
		std::cout << "Update receved for unknown ID: " << id << std::endl;
		return;
	}


	// Get the updated physics state
	PhysicsState state;
	bsIn.Read(state.position);
	bsIn.Read(state.rotation);
	bsIn.Read(state.velocity);
	bsIn.Read(state.angularVelocity);

	// Update the object
	obj->updateState(state, time, RakNet::GetTimeMS(), true);
}



void Client::physicsUpdate()
{
	float deltaTime = (RakNet::GetTimeMS() - lastUpdateTime) * 0.001f;


	//	-----	THIS IS BEING DONE EVERY FRAME	-----
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_CLIENT_INPUT);
	bs.Write(RakNet::GetTimeMS());
	getInput(bs);
	//send input to the server
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	//client object prediction
	//	physics step to get to present (dead reckoning)
	//	get diff from processInputMovement
	//	add diff to buffer
	//	apply diff (input buffer prediction)



	// Update the game objects. This is dead reckoning
	for (auto& it : gameObjects)
	{
		it.second->physicsStep(deltaTime);
	}



	lastUpdateTime = RakNet::GetTimeMS();
}


void Client::processSystemMessage(const RakNet::Packet* packet)
{
	// Check package ID to determine what it is
	switch (packet->data[0])
	{
		// We have ben disconnected
	case ID_DISCONNECTION_NOTIFICATION:
		destroyObjects();
		break;
	case ID_CONNECTION_LOST:
		destroyObjects();
		break;
	


		// These packets are sent when we connect to a server
	case ID_SERVER_SET_CLIENT_ID:
	{
		// Get the client ID and create static objects
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(1);
		bsIn.Read(clientID);
		createStaticObjects(bsIn);
		break;
	}
	case ID_SERVER_CREATE_CLIENT_OBJECT:
	{
		// Use the data to create our client object
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(1);
		createClientObject(bsIn);
		break;
	}

	
	case ID_SERVER_CREATE_GAME_OBJECT:
	{
		// Use the data to create a game object
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(1);
		createGameObject(bsIn);
		break;
	}
	case ID_SERVER_DESTROY_GAME_OBJECT:
	{
		// Get the object ID to destroy
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(1);
		unsigned int id;
		bsIn.Read(id);

		// Delete the object and remove it from the map
		delete gameObjects[id];
		gameObjects.erase(id);

		break;
	}
	
	case ID_SERVER_UPDATE_GAME_OBJECT:
	{
		// Note: these packets are sent unreliably in channel 1
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(1);
		applyServerUpdate(bsIn);
		break;
	}


	default:
		break;
	}
}
