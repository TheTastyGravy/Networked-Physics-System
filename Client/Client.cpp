#include "Client.h"
#include "../Shared/GameMessages.h"
#include <BitStream.h>
#include <iostream>


Client::Client()
{
	peerInterface = RakNet::RakPeerInterface::GetInstance();
	clientID = -1;
}

Client::~Client()
{
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
}

void Client::createClientObject(RakNet::BitStream& bsIn)
{
	//get defined info from packet, then send the rest to user function
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
	delete myClientObject;
	myClientObject = nullptr;
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
		//update the object using the id and timestamp
		//packet structure is from gameobject::serialize
		break;
	}


	default:
		break;
	}
}
