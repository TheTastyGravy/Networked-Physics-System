#include "Client.h"
#include <BitStream.h>
#include <GetTime.h>
#include "../Shared/GameMessages.h"
#include "../Shared/CollisionSystem.h"
#include "../Shared/Sphere.h"
#include "../Shared/OBB.h"


Client::Client(float timeBetweenInputMessages, int maxInputsPerMessage) :
	inputBuffer(RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input, uint32_t>>(30)),
	timeBetweenInputMessages(timeBetweenInputMessages),
	maxInputsPerMessage(maxInputsPerMessage)
{
	peerInterface = RakNet::RakPeerInterface::GetInstance();
	myClientObject = nullptr;
	serverPlayoutDelay = 0;
	lastUpdateTime = RakNet::GetTime();
	lastInputSent = RakNet::GetTime();
	clientID = -1;
	lastInputReceipt = -1;
}

Client::~Client()
{
	peerInterface->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(peerInterface);
	destroyAllObjects();
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
	// All static objects are sent at once, so keep going untill there is no data left
	while (bsIn.GetNumberOfUnreadBits() > 0)
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
			if (obj)
			{
				delete obj;
			}
			if (info.collider)
			{
				delete info.collider;
			}

			// Throw a descriptive error
			std::string str = "Error creating static object with typeID " + std::to_string(typeID);
			throw new std::exception(str.c_str());
		}

		staticObjects.push_back(obj);
	}
}

void Client::createGameObject(RakNet::BitStream& bsIn)
{
	// Read information defined in GameObject.serialize()
	unsigned int objectID;
	bsIn.Read(objectID);
	// Check if the object is blacklisted and should not be created
	for (unsigned int i = 0; i < objectIDBlacklist.size(); i++)
	{
		if (objectID == objectIDBlacklist[i])
		{
			// Remove the object from the blacklist and exit
			objectIDBlacklist.erase(objectIDBlacklist.begin() + i);
			return;
		}
	}

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
	bsIn.Read(info.friction);


	// Use factory method to create object, making sure it was made correctly
	GameObject* obj = gameObjectFactory(typeID, objectID, info, bsIn);
	if (!obj || obj->getID() != objectID)
	{
		if (obj)
		{
			delete obj;
		}
		if (info.collider)
		{
			delete info.collider;
		}

		// Throw a descriptive error
		std::string str = "Error creating game object with typeID " + std::to_string(typeID);
		throw new std::exception(str.c_str());
	}

	gameObjects[objectID] = obj;
}

void Client::createClientObject(RakNet::BitStream& bsIn)
{
	// Get the playout delay
	bsIn.Read(serverPlayoutDelay);
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
	bsIn.Read(info.friction);


	// Use factory method to create object, making sure it was made correctly
	ClientObject* obj = clientObjectFactory(typeID, info, bsIn);
	if (!obj || obj->getID() != clientID)
	{
		if (obj)
		{
			delete obj;
		}
		if (info.collider)
		{
			delete info.collider;
		}

		// Throw a descriptive error
		std::string str = "Error creating client object with typeID " + std::to_string(typeID);
		throw new std::exception(str.c_str());
	}

	myClientObject = obj;
}


void Client::destroyGameObject(unsigned int objectID)
{
	// If the object doesnt exist, add it to the blacklist incase we receved this package early
	if (gameObjects.count(objectID) == 0)
	{
		objectIDBlacklist.push_back(objectID);
		return;
	}

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
		// Lambda function to do collision between the client object and static and game objects, only affecting the client object
		auto collisionFunc = [this]()
		{
			for (auto& gameObjIt = gameObjects.begin(); gameObjIt != gameObjects.end(); gameObjIt++)
			{
				CollisionSystem::handleCollision(myClientObject, gameObjIt->second, false);
			}
			for (auto& staticObj : staticObjects)
			{
				CollisionSystem::handleCollision(myClientObject, staticObj, false);
			}
		};

		// Update object with input buffer. Because we applied the input from the receved state, we are ahead of it by 1/2 RTT plus 
		// a delay the server uses to buffer inputs.
		// Input recieved	|	|
		// and processed	|\	|
		//					| \	|
		//					|  \| Server recieves input
		//					|	|
		//					|	| After delay, server processes 
		//					|  /| input and sends update
		//					| / |
		// We finaly get	|/	|
		// the updated state|	|
		int ping = peerInterface->GetAveragePing(peerInterface->GetSystemAddressFromIndex(0));
		myClientObject->updateStateWithInputBuffer(state, timeStamp - (ping * 0.5f) - serverPlayoutDelay, RakNet::GetTime(), inputBuffer, true, collisionFunc);
	}
	else if (gameObjects.count(id) > 0)	//gameObjects has more than 0 entries of id
	{
		// Update the game object, with smoothing
		gameObjects[id]->updateState(state, timeStamp, timeStamp/*RakNet::GetTime()*/, true);
	}
}

void Client::checkAckReceipt(RakNet::BitStream& bsIn)
{
	uint32_t recievedNum;
	bsIn.Read(recievedNum);

	bool foundLast = false;
	bool foundRecieved = false;
	for (unsigned short i = 0; i < inputBuffer.getSize(); i++)
	{
		// Get the allocated receipt number for the message
		uint32_t num = std::get<3>(inputBuffer[i]);

		if (num == recievedNum)
		{
			// The last number is older than the received number
			if (foundLast)
			{
				lastInputReceipt = recievedNum;
				return;
			}

			foundRecieved = true;
		}
		else if (num == lastInputReceipt)
		{
			// The recieved number is older than the last number
			if (foundRecieved)
			{
				return;
			}

			foundLast = true;
		}
	}

	// The last receipt wasnt found but the new one was, so use the new one
	if (foundRecieved && !foundLast)
	{
		lastInputReceipt = recievedNum;
	}
}

void Client::sendInput()
{
	// Get time of the most recent input
	RakNet::Time time = std::get<0>(inputBuffer[inputBuffer.getSize() - 1]);
	// Get the receipt number this message will use
	uint32_t nextReceipt = peerInterface->GetNextSendReceipt();


	RakNet::BitStream bs;
	// Writing the time stamp first allows raknet to convert local times between systems
	bs.Write((RakNet::MessageID)ID_TIMESTAMP);
	bs.Write(time);
	bs.Write((RakNet::MessageID)ID_CLIENT_INPUT);

	// Recursive lambda function to write unacked inputs oldest to newest
	std::function<void(int, int)> writeBackwardOrder = nullptr;
	writeBackwardOrder = [&](int index, int remaining)
	{
		if (index < 0 || remaining <= 0)
		{
			return;
		}

		auto& entry = inputBuffer[index];
		// If the entry hasnt been sent yet, assign its receipt
		uint32_t& entryReceipt = std::get<3>(entry);
		if (entryReceipt == -1)
		{
			entryReceipt = nextReceipt;
		}

		// Only write inputs that havent been acked
		if (entryReceipt != lastInputReceipt)
		{
			// Recursion
			writeBackwardOrder(index - 1, remaining - 1);
			// Time offset, input
			bs.Write(time - std::get<0>(entry));
			bs.Write(std::get<2>(entry));
		}
	};

	// Write inputs to the message and send it
	writeBackwardOrder(inputBuffer.getSize() - 1, maxInputsPerMessage);
	peerInterface->Send(&bs, HIGH_PRIORITY, UNRELIABLE_WITH_ACK_RECEIPT, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	lastInputSent = RakNet::GetTime();
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
				CollisionSystem::handleCollision(gameObj, staticObj, true);
			}
			// Other game objects
			for (auto& otherGameObjIt = std::next(gameObjIt); otherGameObjIt != gameObjects.end(); otherGameObjIt++)
			{
				CollisionSystem::handleCollision(gameObj, otherGameObjIt->second, true);
			}

			// Our client object
			if (myClientObject)
			{
				CollisionSystem::handleCollision(gameObj, myClientObject, true);
			}
		}

		// Static objects with our client object
		if (myClientObject)
		{
			for (auto& staticObj : staticObjects)
			{
				CollisionSystem::handleCollision(myClientObject, staticObj, true);
			}
		}
	}
	

	// Update our client object, get input, and predict it localy
	if (myClientObject != nullptr)
	{
		// Get player input and push it to the input buffer with its physics state
		Input input = getInput();
		inputBuffer.push(std::make_tuple(currentTime, myClientObject->getCurrentState(), input, -1));

		// Get and then apply the diff state from the input
		PhysicsState diff = myClientObject->processInputMovement(input);
		myClientObject->applyStateDiff(diff, currentTime, currentTime);
		// Use action input for prediction
		myClientObject->processInputAction(input, currentTime);

		// Update to the current time
		myClientObject->physicsStep(deltaTime);

		// If enough time has passed, send input to the server
		if (currentTime - lastInputSent >= RakNet::Time(timeBetweenInputMessages * 1000))
		{
			sendInput();
		}
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
		destroyAllObjects();
		break;
	case ID_CONNECTION_LOST:
		destroyAllObjects();
		break;

		// The server has recieved one of out messages
	case ID_SND_RECEIPT_ACKED:
		checkAckReceipt(bsIn);
		break;
	

		// These packets are sent when we connect to a server
	case ID_SERVER_CREATE_STATIC_OBJECTS:
		createStaticObjects(bsIn);
		break;
	case ID_SERVER_CREATE_CLIENT_OBJECT:
		createClientObject(bsIn);
		break;


	case ID_SERVER_CREATE_GAME_OBJECT:
		createGameObject(bsIn);
		break;
	case ID_SERVER_DESTROY_GAME_OBJECT:
	{
		// This message only has the objects ID
		unsigned int id;
		bsIn.Read(id);
		destroyGameObject(id);
		break;
	}
	case ID_SERVER_UPDATE_GAME_OBJECT:
		// Note: these packets are sent unreliably in channel 1
		// This packet is sent with a timestamp, which we already got
		applyServerUpdate(bsIn, time);
		break;


	default:
		break;
	}
}
