#include "Client.h"
#include "../Shared/GameMessages.h"
#include <BitStream.h>
#include <GetTime.h>
#include "../Shared/CollisionSystem.h"
#include "../Shared/Sphere.h"
#include "../Shared/OBB.h"

#include <iostream>


Client::Client() :
	inputBuffer(RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input>>(30))
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
	destroyAllObjects();
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

	//peerInterface->ApplyNetworkSimulator(0.1f, 50, 50);

    std::cout << "Connecting to server at: " << ip << std::endl;

    // Attempt to connect to the server
    RakNet::ConnectionAttemptResult res = peerInterface->Connect(ip, port, nullptr, 0);
    if (res != RakNet::CONNECTION_ATTEMPT_STARTED)
    {
        std::cout << "Unable to start connection, error number: " << res << std::endl;
    }
}



Collider* Client::readCollider(RakNet::BitStream& bsIn)
{
	// Use the shape ID to determine what collider to create
	int shapeID;
	bsIn.Read(shapeID);

	switch (shapeID)
	{
	case 0:	//Sphere
	{
		float radius;
		bsIn.Read(radius);
		return new Sphere(radius);
	}
	case 1:	//OBB
	{
		raylib::Vector3 extents;
		bsIn.Read(extents);
		return new OBB(extents);
	}

	default:
		return nullptr;
	}
}

void Client::createStaticObjects(RakNet::BitStream& bsIn)
{
	// Read information defined in StaticObject.serialize()
	int typeID;
	bsIn.Read(typeID);
	ObjectInfo info;

	info.collider = readCollider(bsIn);

	bsIn.Read(info.state.position);
	bsIn.Read(info.state.rotation);


	// Use factory method to create object, making sure it was made correctly
	StaticObject* obj = staticObjectFactory(typeID, info, bsIn);
	if (!obj)
	{
		std::cout << "Error creating static object with typeID " << typeID << std::endl;

		if (obj)
		{
			delete obj;
		}
		return;
	}

	staticObjects.push_back(obj);
}

void Client::createGameObject(RakNet::BitStream& bsIn)
{
	// Read information defined in GameObject.serialize()
	unsigned int objectID;
	bsIn.Read(objectID);
	int typeID;
	bsIn.Read(typeID);
	ObjectInfo info;

	info.collider = readCollider(bsIn);

	bsIn.Read(info.state.position);
	bsIn.Read(info.state.rotation);
	bsIn.Read(info.state.velocity);
	bsIn.Read(info.state.angularVelocity);
	bsIn.Read(info.mass);
	bsIn.Read(info.elasticity);


	// Use factory method to create object, making sure it was made correctly
	GameObject* obj = gameObjectFactory(typeID, objectID, info, bsIn);
	if (!obj || obj->getID() != objectID)
	{
		std::cout << "Error creating game object with typeID " << typeID << std::endl;

		if (obj)
		{
			delete obj;
		}
		return;
	}

	gameObjects[objectID] = obj;
}

void Client::createClientObject(RakNet::BitStream& bsIn)
{
	// The object ID is also our client ID
	bsIn.Read(clientID);
	// Read information defined in GameObject.serialize()
	int typeID;
	bsIn.Read(typeID);
	ObjectInfo info;

	info.collider = readCollider(bsIn);

	bsIn.Read(info.state.position);
	bsIn.Read(info.state.rotation);
	bsIn.Read(info.state.velocity);
	bsIn.Read(info.state.angularVelocity);
	bsIn.Read(info.mass);
	bsIn.Read(info.elasticity);


	// Use factory method to create object, making sure it was made correctly
	ClientObject* obj = clientObjectFactory(typeID, info, bsIn);
	if (!obj || obj->getID() != clientID)
	{
		std::cout << "Error creating client object with typeID " << typeID << std::endl;

		if (obj)
		{
			delete obj;
		}
		return;
	}

	myClientObject = obj;
}


void Client::destroyGameObject(unsigned int objectID)
{
	// Delete the object and remove it from the map
	delete gameObjects[objectID];
	gameObjects.erase(objectID);
}

void Client::destroyAllObjects()
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
		gameObjects[id]->updateState(state, timeStamp, timeStamp/*RakNet::GetTime()*/, true);
	}
	else
	{
		// We dont have an object with that ID
		std::cout << "Update receved for unknown ID: " << id << std::endl;
	}
}



void Client::systemUpdate()
{
	RakNet::Time currentTime = RakNet::GetTime();
	float deltaTime = (currentTime - lastUpdateTime) * 0.001f;


	// Predict collisions
	{
		// Game objects with static, other game objects, and our client object
		for (auto& gameObjIt = gameObjects.begin(); gameObjIt != gameObjects.end(); gameObjIt++)
		{
			GameObject* gameObj = gameObjIt->second;

			// Static objects
			for (auto& staticObj : staticObjects)
			{
				CollisionSystem::handleCollision(gameObj, staticObj, false, true);
			}
			// Other game objects
			for (auto& otherGameObjIt = std::next(gameObjIt); otherGameObjIt != gameObjects.end(); otherGameObjIt++)
			{
				CollisionSystem::handleCollision(gameObj, otherGameObjIt->second, false, true);
			}
			// Our client object
			CollisionSystem::handleCollision(gameObj, myClientObject, false, true);
		}

		// Static objects with our client object
		for (auto& staticObj : staticObjects)
		{
			CollisionSystem::handleCollision(myClientObject, staticObj, false, true);
		}
	}
	

	// Update our client object, get input, send it to the server, and predict it localy
	if (myClientObject != nullptr)
	{
		// Update to the current time
		myClientObject->physicsStep(deltaTime);
		// Get player input and push it onto the buffer, with the state before
		Input input = getInput();
		inputBuffer.push(std::make_tuple(currentTime, myClientObject->getCurrentState(), input));


		RakNet::BitStream bs;
		// Writing the time stamp first allows raknet to convert local times between systems
		bs.Write((RakNet::MessageID)ID_TIMESTAMP);
		bs.Write(currentTime);
		bs.Write((RakNet::MessageID)ID_CLIENT_INPUT);
		bs.Write(input);
		// Send input to the server
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

		// Get and then apply the diff state from the input
		PhysicsState diff = myClientObject->processInputMovement(input);
		myClientObject->applyStateDiff(diff, currentTime, currentTime);
	}
	

	// Update the game objects. This is dead reckoning
	//when lag compensation is added, game objects should be updated to time - half ping
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
		destroyAllObjects();
		break;
	case ID_CONNECTION_LOST:
		destroyAllObjects();
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
		// This message only has the objects ID
		unsigned int id;
		bsIn.Read(id);
		destroyGameObject(id);
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
