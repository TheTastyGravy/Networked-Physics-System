#include "Client.h"
#include "../Shared/GameMessages.h"
#include <BitStream.h>
#include <GetTime.h>
#include <iostream>


Client::Client()
{
	peerInterface = RakNet::RakPeerInterface::GetInstance();
	myClientObject = nullptr;
	lastUpdateTime = RakNet::GetTime();
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

	//our client object contains our client ID
	bsIn.Read(clientID);

	//ignore type id for now
	bsIn.IgnoreBytes(sizeof(unsigned int));

	raylib::Vector3 position;
	raylib::Vector3 rotation;
	bsIn.Read(position);
	bsIn.Read(rotation);

	myClientObject = new ClientObject(position, rotation, clientID, 1);
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


void Client::applyServerUpdate(RakNet::BitStream& bsIn, const RakNet::Time& timeStamp)
{
	// Get object ID
	unsigned int id;
	bsIn.Read(id);

	// Get the updated physics state
	PhysicsState state;
	bsIn.Read(state.position);
	bsIn.Read(state.rotation);
	bsIn.Read(state.velocity);
	bsIn.Read(state.angularVelocity);


	if (id == clientID)
	{
		// Because we applied the input from the receved state, we are 1 RTT ahead of this state
		// It took 1/2 RTT for this packet to get to us, so we add the other half
		int halfPing = peerInterface->GetLastPing(peerInterface->GetSystemAddressFromIndex(0)) / 2;

		// Update myClientObject with input buffer
		myClientObject->updateStateWithInputBuffer(state, timeStamp - halfPing, RakNet::GetTime(), inputBuffer, true);
	}
	else if (gameObjects.count(id) > 0)	//gameObjects has more than 0 entries of id
	{
		// Update the game object, with smoothing
		gameObjects[id]->updateState(state, timeStamp, RakNet::GetTime(), true);
	}
	else
	{
		// We dont have an object with that ID
		std::cout << "Update receved for unknown ID: " << id << std::endl;
	}
}



void Client::physicsUpdate()
{
	RakNet::Time currentTime = RakNet::GetTime();
	float deltaTime = (currentTime - lastUpdateTime) * 0.001f;


	//input is being sent to the server every frame right now

	// Get player input and push it onto the buffer
	Input input = getInput();
	inputBuffer.push({ currentTime, input });

	RakNet::BitStream bs;
	// Writing the time stamp first allows raknet to convert local times between systems
	bs.Write((RakNet::MessageID)ID_TIMESTAMP);
	bs.Write(currentTime);
	bs.Write((RakNet::MessageID)ID_CLIENT_INPUT);
	bs.Write(input);
	// Send input to the server
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	if (myClientObject != nullptr)
	{
		// Update to the current time
		myClientObject->physicsStep(deltaTime);
		// Get and then apply the diff state from the input
		PhysicsState diff = myClientObject->processInputMovement(input);
		myClientObject->applyStateDiff(diff, currentTime, currentTime);
	}
	

	// Update the game objects. This is dead reckoning
	for (auto& it : gameObjects)
	{
		it.second->physicsStep(deltaTime);
	}

	lastUpdateTime = currentTime;
}


void Client::processSystemMessage(const RakNet::Packet* packet)
{
	RakNet::BitStream bsIn(packet->data, packet->length, false);

	// Get the message ID
	RakNet::MessageID messageID;
	bsIn.Read(messageID);

	// If this packet has a time stamp, get it
	RakNet::Time time;
	if (messageID == ID_TIMESTAMP)
	{
		// Get the time stamp, then update the message ID
		bsIn.Read(time);
		bsIn.Read(messageID);
	}


	// Check package ID to determine what it is
	switch (messageID)
	{
		// We have ben disconnected
	case ID_DISCONNECTION_NOTIFICATION:
		destroyObjects();
		break;
	case ID_CONNECTION_LOST:
		destroyObjects();
		break;
	

		// These packets are sent when we connect to a server
	case ID_SERVER_CREATE_STATIC_OBJECTS:
	{
		// Create static objects
		createStaticObjects(bsIn);
		break;
	}
	case ID_SERVER_CREATE_CLIENT_OBJECT:
	{
		// Use the data to create our client object and get our client ID
		createClientObject(bsIn);
		break;
	}

	
	case ID_SERVER_CREATE_GAME_OBJECT:
	{
		// Use the data to create a game object
		createGameObject(bsIn);
		break;
	}
	case ID_SERVER_DESTROY_GAME_OBJECT:
	{
		// Get the object ID to destroy
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
		// This packet is sent with a timestamp, which we already got
		applyServerUpdate(bsIn, time);
		break;
	}


	default:
		break;
	}
}