#include "Server.h"
#include <GetTime.h>
#include "../Shared/GameMessages.h"
#include "../Shared/CollisionSystem.h"


Server::Server(float timeStep, float playoutDelay) :
	timeStep(timeStep * 1000),
	playoutDelay(playoutDelay * 1000)
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
		processInput(packet->systemAddress, bsIn);
		break;


	default:
		break;
	}
}


void Server::systemUpdate()
{
	// Use static to keep value between calls
	static RakNet::Time accumulatedTime = 0;


	RakNet::Time frameTime = RakNet::GetTime();
	accumulatedTime += frameTime - lastUpdateTime;
	// Time for the fixed time step
	RakNet::Time tickTime = lastUpdateTime - (lastUpdateTime % timeStep);

	// Fixed time step for physics
	while (accumulatedTime >= timeStep)
	{
		collisionDetectionAndResolution();

		// Update client and game objects
		for (auto& it : clientObjects)
		{
			updateClientObject(it.first, tickTime - playoutDelay);
		}
		for (auto& it : gameObjects)
		{
			it.second->physicsStep(timeStep * 0.001f);
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
		tickTime += timeStep;
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
	// Setup playout buffer
	PlayoutBuffer& playoutBuffer = playoutBuffers[nextClientID];
	playoutBuffer.timeBetweenFrames = 1000 / 60;	//60 fps


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
	playoutBuffers.erase(id);
	addressToClientID.erase(RakNet::SystemAddress::ToInteger(disconnectedAddress));
}


void Server::processInput(const RakNet::SystemAddress& address, RakNet::BitStream& bsIn)
{
	// Get client ID from address, and check its valid
	unsigned int clientID = addressToClientID[RakNet::SystemAddress::ToInteger(address)];
	if (clientID == 0)
	{
		return;
	}

	PlayoutBuffer& playout = playoutBuffers[clientID];
	ClientObject* clientObject = clientObjects[clientID];


	// The frame of the newest input we have
	unsigned int lastFrame;
	if (playout.buffer.empty())
	{
		// This is the first input we have recieved from the client
		playout.initTime = RakNet::GetTime();
		lastFrame = 0;
	}
	else
	{
		// Use the frame of the last input in the buffer
		lastFrame = playout.buffer.back().frame;
	}


	// Read in all inputs
	while (bsIn.GetNumberOfUnreadBits() > 0)
	{
		unsigned int frame;
		Input input;
		bsIn.Read(frame);
		bsIn.Read(input);

		// Input is more recent than the last input in the buffer
		if (frame > lastFrame)
		{
			playout.buffer.push_back(PlayoutBuffer::InputData(frame, input, false));
		}
	}
}

void Server::updateClientObject(unsigned int clientID, const RakNet::Time& time)
{
	ClientObject* clientObj = clientObjects[clientID];
	PlayoutBuffer& playout = playoutBuffers[clientID];

	// If this is before the init time, exit. This can happen due to the playout delay
	if (time < playout.initTime)
	{
		return;
	}


	// current frame = time passed since init / time per frame
	playout.currentFrame = (time - playout.initTime) / playout.timeBetweenFrames;

	// Use inputs while their frame is less than the current frame
	while (!playout.buffer.empty())
	{
		// Get input info
		PlayoutBuffer::InputData& inputData = playout.buffer.front();

		// Input is in the future, do nothing
		if (inputData.frame > playout.currentFrame)
		{
			break;
		}

		// Calculate the time of the input relitive to frame 0
		RakNet::Time inputTime = playout.initTime + inputData.frame * playout.timeBetweenFrames;
		// Apply movement input
		PhysicsState inputDiff = clientObj->processInputMovement(inputData.input);
		clientObj->applyStateDiff(inputDiff, inputTime, inputTime, false, true);
		// Only use action inputs once
		if (!inputData.actionHasBeenPerformed)
		{
			inputData.actionHasBeenPerformed = true;
			clientObj->processInputAction(inputData.input, inputTime);
		}
		
		// Check if the next input can be used
		if (playout.buffer.size() >= 2 && playout.buffer[1].frame <= playout.currentFrame)
		{
			// Remove this input from the buffer and continue to use the next input
			playout.buffer.pop_front();
			continue;
		}
		break;
	}

	// Update physics state
	clientObj->physicsStep(timeStep * 0.001f);
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
	// If the object is a client object, write the current frame. Only used by the client that owns it
	if (object->getID() < nextClientID)
	{
		bs.Write(playoutBuffers[object->getID()].currentFrame);
	}

	// Send the packet to all clients. It is not garenteed to arrive, but are sent often
	peerInterface->Send(&bs, MEDIUM_PRIORITY, UNRELIABLE, 1, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}
