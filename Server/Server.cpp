#include "Server.h"
#include <GetTime.h>
#include "../Shared/GameMessages.h"
#include "../Shared/CollisionSystem.h"


Server::Server(float timeStep, float playoutDelay) :
	timeStep(timeStep),
	playoutDelay(playoutDelay)
{
	peerInterface = RakNet::RakPeerInterface::GetInstance();
	lastUpdateTime = RakNet::GetTime();
}

Server::~Server()
{
	RakNet::RakPeerInterface::DestroyInstance(peerInterface);


	for (auto& it : staticObjects)
	{
		delete it;
	}
	staticObjects.clear();

	for (auto& it : gameObjects)
	{
		delete it.second;
	}
	gameObjects.clear();

	for (auto& it : clientObjects)
	{
		delete it.second;
	}
	clientObjects.clear();

	addressToClientID.clear();
}


void Server::createObject(unsigned int typeID, const PhysicsState& state, const RakNet::Time& creationTime, RakNet::BitStream* customParamiters)
{
	// Create a state to fix time diference
	float deltaTime = (RakNet::GetTime() - creationTime) * 0.001f;
	PhysicsState newState(state);
	newState.position += newState.velocity * deltaTime;
	newState.rotation += newState.angularVelocity * deltaTime;

	// Use factory method to create new object, checking it was created correctly
	GameObject* obj = gameObjectFactory(typeID, nextObjectID, newState, *customParamiters);
	if (!obj || obj->getID() != nextObjectID)
	{
		if (obj)
		{
			delete obj;
		}

		// Throw a descriptive error
		std::string str = "Error creating object with typeID " + std::to_string(typeID);
		throw new std::exception(str.c_str());
	}


	// Send reliable message to clients for them to create the object
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_CREATE_GAME_OBJECT);
	obj->serialize(bs);
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	gameObjects[nextObjectID] = obj;
	nextObjectID++;
}

void Server::destroyObject(unsigned int objectID)
{
	// Only destroy game objects
	if (objectID < nextClientID)
	{
		return;
	}

	// Add the object ID to the list of objects to be destroied
	deadObjects.push_back(objectID);
}


void Server::processSystemMessage(const RakNet::Packet* packet)
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
		// New client is connecting
	case ID_NEW_INCOMING_CONNECTION:
		onClientConnect(packet->systemAddress);
		break;

		// A client has disconnected
	case ID_DISCONNECTION_NOTIFICATION:
		onClientDisconnect(packet->systemAddress);
		break;
	case ID_CONNECTION_LOST:
		onClientDisconnect(packet->systemAddress);
		break;


		// A client has sent us their player input
	case ID_CLIENT_INPUT:
		processInput(packet->systemAddress, bsIn, time);
		break;


	default:
		break;
	}
}


void Server::systemUpdate()
{
	// Use static to keep value between calls
	static float accumulatedTime = 0.0f;

	// Time step as RakNet::Time
	RakNet::Time absoluteTimeStep = RakNet::Time(timeStep * 1000);
	RakNet::Time absolutePlayoutDelay = RakNet::Time(playoutDelay * 1000);

	RakNet::Time frameTime = RakNet::GetTime();
	accumulatedTime += (frameTime - lastUpdateTime) * 0.001f;
	// Time for the fixed time step
	RakNet::Time currentTime = lastUpdateTime - (lastUpdateTime % absoluteTimeStep);

	// Fixed time step for physics
	while (accumulatedTime >= timeStep)
	{
		collisionDetectionAndResolution();

		// Update client and game objects
		for (auto& it : clientObjects)
		{
			updateClientObject(it.first, currentTime - absolutePlayoutDelay);
		}
		for (auto& it : gameObjects)
		{
			it.second->physicsStep(timeStep);
		}

		// Destroy objects
		for (auto id : deadObjects)
		{
			if (gameObjects.count(id) > 0)	// Make sure the object still exists
			{
				// Send message to clients
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)ID_SERVER_DESTROY_GAME_OBJECT);
				bs.Write(id);
				peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

				// Delete the object
				delete gameObjects[id];
				gameObjects.erase(id);
			}
		}
		deadObjects.clear();

		// Update time
		accumulatedTime -= timeStep;
		currentTime += absoluteTimeStep;
	}


	// Send updated states
	for (auto& it : gameObjects)
	{
		sendGameObjectUpdate(it.second, frameTime);
	}
	for (auto& it : clientObjects)
	{
		sendGameObjectUpdate(it.second, frameTime);
	}

	// Update time now that this update is over
	lastUpdateTime = frameTime;
}

void Server::collisionDetectionAndResolution()
{
	// Game objects with static, client, and other game objects
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
		// Client objects
		for (auto& clientObjIt = clientObjects.begin(); clientObjIt != clientObjects.end(); clientObjIt++)
		{
			CollisionSystem::handleCollision(gameObj, clientObjIt->second, true);
		}
	}

	// Client objects with static and other client objects
	for (auto& clientObjIt = clientObjects.begin(); clientObjIt != clientObjects.end(); clientObjIt++)
	{
		GameObject* gameObj = clientObjIt->second;

		// Static objects
		for (auto& staticObj : staticObjects)
		{
			CollisionSystem::handleCollision(gameObj, staticObj, true);
		}
		// Other client objects
		for (auto& otherClientObjIt = std::next(clientObjIt); otherClientObjIt != clientObjects.end(); otherClientObjIt++)
		{
			CollisionSystem::handleCollision(gameObj, otherClientObjIt->second, true);
		}
	}
}


void Server::onClientConnect(const RakNet::SystemAddress& connectedAddress)
{
	// Add client to map
	addressToClientID[RakNet::SystemAddress::ToInteger(connectedAddress)] = nextClientID;


	// Send static objects
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_STATIC_OBJECTS);
		// Add each static object
		for (auto& it : staticObjects)
		{
			// If the packet is almost full, send it and start a new one to prevent fragmenting
			if (bs.GetNumberOfBytesUsed() > peerInterface->GetMTUSize(RakNet::UNASSIGNED_SYSTEM_ADDRESS) * 0.95f)
			{
				peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
				bs.Reset();
				bs.Write((RakNet::MessageID)ID_SERVER_CREATE_STATIC_OBJECTS);
			}

			it->serialize(bs);
		}

		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
	}

	// Send existing game and client objects
	for (auto& it : gameObjects)
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_GAME_OBJECT);
		it.second->serialize(bs);
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
	}
	for (auto& it : clientObjects)
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_GAME_OBJECT);
		it.second->serialize(bs);
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
	}


	// Create client object and add it to the map
	ClientObject* clientObject = clientObjectFactory(nextClientID);
	if (!clientObject ||  clientObject->getID() != nextClientID)
	{
		if (clientObject)
		{
			delete clientObject;
		}

		// Throw a descriptive error
		std::string str = "Error creating client object for clientID " + std::to_string(nextClientID);
		throw new std::exception(str.c_str());
	}
	clientObjects[nextClientID] = clientObject;
	// Send client object to client
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_CLIENT_OBJECT);
		bs.Write(RakNet::Time(playoutDelay * 1000));	// Send the playout delay to clients for prediction
		clientObject->serialize(bs);
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
	}
	// Send game object to all other clients
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_GAME_OBJECT);
		clientObject->serialize(bs);
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, true);
	}


	nextClientID++;
}

void Server::onClientDisconnect(const RakNet::SystemAddress& disconnectedAddress)
{
	unsigned int id = addressToClientID[RakNet::SystemAddress::ToInteger(disconnectedAddress)];

	// Send message to all clients to destroy the disconnected clients object
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_DESTROY_GAME_OBJECT);
	bs.Write(id);
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	// Remove the client object, its playout buffer, and its address from the map
	delete(clientObjects[id]);
	clientObjects.erase(id);
	playoutBuffer.erase(id);
	addressToClientID.erase(RakNet::SystemAddress::ToInteger(disconnectedAddress));
}


void Server::processInput(const RakNet::SystemAddress& address, RakNet::BitStream& bsIn, const RakNet::Time& timeStamp)
{
	// Get client ID from address, and check its valid
	unsigned int clientID = addressToClientID[RakNet::SystemAddress::ToInteger(address)];
	if (clientID == 0)
	{
		return;
	}

	// Adjust the time stamp by 1/2 latency to account for the time taken. The effect is 
	// that it approximatly matches 'now' localy
	RakNet::Time baseTime = timeStamp + peerInterface->GetAveragePing(address) / 2;

	// The time of the newest input we have
	RakNet::Time lastTime;
	if (playoutBuffer[clientID].empty())
	{
		// Edge case. Should only happen for the first inputs received
		lastTime = 0;
	}
	else
	{
		// Get the time of the newest input, and exit if the message is older
		lastTime = std::get<0>(playoutBuffer[clientID].back());
		if (baseTime < lastTime)
		{
			return;
		}
	}

	ClientObject* clientObject = clientObjects[clientID];

	// Read in all inputs
	while (bsIn.GetNumberOfUnreadBits() > 0)
	{
		RakNet::Time offset;
		Input input;
		bsIn.Read(offset);
		bsIn.Read(input);

		// Input is more recent than the last input in the buffer
		if (baseTime - offset > lastTime)
		{
			playoutBuffer[clientID].push_back(std::make_tuple(baseTime - offset, input, false));
		}
	}
}

void Server::updateClientObject(unsigned int clientID, const RakNet::Time& time)
{
	ClientObject* clientObj = clientObjects[clientID];
	
	// Use inputs while their time stamp is less than the current time
	auto& inputs = playoutBuffer[clientID];
	while (!inputs.empty())
	{
		// Get input info
		RakNet::Time inputTime = std::get<0>(inputs.front());
		Input& input = std::get<1>(inputs.front());
		bool& actionPerformed = std::get<2>(inputs.front());

		// Input is in the future, do nothing
		if (inputTime > time)
		{
			break;
		}

		// Apply movement input
		PhysicsState inputDiff = clientObj->processInputMovement(input);
		clientObj->applyStateDiff(inputDiff, inputTime, inputTime, false, true);
		// Only use action inputs once
		if (!actionPerformed)
		{
			actionPerformed = true;
			clientObj->processInputAction(input, inputTime);
		}
		
		// Check if the next input can be used
		if (inputs.size() >= 2 && std::get<0>(inputs[1]) <= time)
		{
			// Remove this input from the buffer and continue to use the next input
			inputs.pop_front();
			continue;
		}
		break;
	}

	// Update physics state
	clientObj->physicsStep(timeStep);
}


void Server::sendGameObjectUpdate(GameObject* object, RakNet::Time timeStamp)
{
	RakNet::BitStream bs;

	// Writing the time stamp first allows raknet to convert local times between systems
	bs.Write((RakNet::MessageID)ID_TIMESTAMP);
	bs.Write(timeStamp);

	bs.Write((RakNet::MessageID)ID_SERVER_UPDATE_GAME_OBJECT);
	bs.Write(object->getID());
	bs.Write(object->getPosition());
	bs.Write(object->getRotation());
	bs.Write(object->getVelocity());
	bs.Write(object->getAngularVelocity());

	// Send the packet to all clients. It is not garenteed to arrive, but are sent often
	peerInterface->Send(&bs, MEDIUM_PRIORITY, UNRELIABLE, 1, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}
