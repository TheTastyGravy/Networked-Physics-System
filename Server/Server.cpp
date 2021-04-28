#include "Server.h"
#include "../Shared/GameMessages.h"
#include <GetTime.h>



Server::Server()
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


void Server::destroyObject(unsigned int objectID)
{
	// Only destroy game objects
	if (objectID < nextClientID)
	{
		return;
	}

	// Packet only contains objectID to destroy
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_DESTROY_GAME_OBJECT);
	bs.Write(objectID);
	// Send packet to all clients, reliably
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	// Destroy the object
	delete gameObjects[objectID];
	gameObjects.erase(objectID);
}

void Server::createObject(unsigned int typeID, const PhysicsState& state, const RakNet::Time& creationTime, RakNet::BitStream& customParamiters)
{
	// Create a state to fix time diference
	float deltaTime = (RakNet::GetTime() - creationTime) * 0.001f;
	PhysicsState newState(state);
	newState.position += newState.velocity * deltaTime;
	newState.rotation += newState.angularVelocity * deltaTime;

	// Use factory method to create new object
	GameObject* obj = gameObjectFactory(typeID, nextObjectID, newState, customParamiters);

	// If the object was created wrong, display message and destroy it
	if (!obj || obj->getID() != nextObjectID)
	{
		std::cout << "Error creating object with typeID " << typeID << std::endl;

		if (obj)
		{
			delete obj;
		}
		return;
	}


	// Send reliable message to clients for them to create the object
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_CREATE_GAME_OBJECT);
	obj->serialize(bs);
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	gameObjects[nextObjectID] = obj;
	nextObjectID++;
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
	{
		unsigned int id = addressToClientID[RakNet::SystemAddress::ToInteger(packet->systemAddress)];
		if (id == 0)
		{
			break;
		}

		processInput(id, bsIn, time);
		break;
	}


	default:
		break;
	}
}


void Server::physicsUpdate()
{
	//could implement fixed time step, but for now leave it

	collisionDetectionAndResolution();

	RakNet::Time currentTime = RakNet::GetTime();
	float deltaTime = (currentTime - lastUpdateTime) * 0.001f;

	// Update game objects, sending updates to clients
	for (auto& it : gameObjects)
	{
		it.second->physicsStep(deltaTime);
		sendGameObjectUpdate(it.second, currentTime);
	}

	// Update time now that this update is over
	lastUpdateTime = currentTime;
}

void Server::collisionDetectionAndResolution()
{
	auto applyContactForces = [](GameObject* obj1, StaticObject* obj2, raylib::Vector3 collisionNorm, float pen)
	{
		// Try to cast the second object to a game object
		GameObject* gameObj2 = nullptr;
		if (!obj2->isStatic())
		{
			gameObj2 = static_cast<GameObject*>(obj2);
		}


		// If no obj2 was passed, use 'infinite' mass
		float body2Mass = gameObj2 ? gameObj2->getMass() : INT_MAX;
		float body1Factor = body2Mass / (obj1->getMass() + body2Mass);


		// Apply contact forces
		obj1->position -= collisionNorm * pen * body1Factor;
		if (gameObj2)
		{
			gameObj2->position += collisionNorm * pen * (1 - body1Factor);
		}
	};


	for (auto& gameObj = gameObjects.begin(); gameObj != gameObjects.end(); gameObj++)
	{
		//iterate through static objects
		for (auto& staticObj : staticObjects)
		{
			//check collision

			//if colliding, resolve
			//use resolve info for contact forces
		}

		//iterate through game objects after the current one
		for (auto& otherGameObj = std::next(gameObj); otherGameObj != gameObjects.end(); otherGameObj++)
		{
			//check collision

			//if colliding, resolve
			//use resolve info for contact forces
		}
	}
}



void Server::onClientConnect(const RakNet::SystemAddress& connectedAddress)
{
	std::cout << "Client " << nextClientID << " has connected" << std::endl;

	// Add client to map
	addressToClientID[RakNet::SystemAddress::ToInteger(connectedAddress)] = nextClientID;


	//send static objects
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_STATIC_OBJECTS);
		//add each static object
		for (auto& it : staticObjects)
		{
			it->serialize(bs);
		}
		//send the message
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
	}


	//send existing game objects
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


	//create object and add it to the map
	ClientObject* clientObject = clientObjectFactory(nextClientID);
	clientObjects[nextClientID] = clientObject;
	//send client object to client
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_CREATE_CLIENT_OBJECT);
		clientObject->serialize(bs);
		peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, connectedAddress, false);
	}
	//send game object to all other clients
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

	std::cout << "Client " << id << " has disconnected" << std::endl;

	// Send message to all clients to destroy the disconnected clients object
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_DESTROY_GAME_OBJECT);
	bs.Write(id);
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	// Remove the client object and its address from the map
	clientObjects.erase(id);
	addressToClientID.erase(RakNet::SystemAddress::ToInteger(disconnectedAddress));
}


void Server::processInput(unsigned int clientID, RakNet::BitStream& bsIn, const RakNet::Time& timeStamp)
{
	ClientObject* clientObject = clientObjects[clientID];

	// Get the input struct. Input is defined in ClientObject.h
	Input input;
	bsIn.Read(input);


	// Note: a good improvement to make is having clients send past inputs with their current one, 
	// so if an input is dropped or out of order, the next packet will have it anyway. an issue with 
	// this is time stamps will only be converted by raknet if its at the start of a packet, so the 
	// time stamp for each input would have to be relitive to that, requiering clients to have a packet 
	// manager for sending inputs


	// Action inputs can always be used, since they dont affect physics state
	//clientObject->processInputAction(input, timeStamp);


	// If the input is older than one we have already receved, dont use it for movement
	if (timeStamp < clientObject->getTime())
	{
		return;
	}


	// Update the object up to the time of the receved input
	float deltaTime = (timeStamp - clientObject->getTime()) * 0.001f;
	clientObject->physicsStep(deltaTime);

	// Process the input, getting a state diff
	PhysicsState inputDiff = clientObject->processInputMovement(input);

	// Apply the diff to the object
	clientObject->applyStateDiff(inputDiff, timeStamp, timeStamp, false, true);
	

	// Send update to clients
	sendGameObjectUpdate(clientObject, RakNet::GetTime());
}



void Server::sendGameObjectUpdate(GameObject* object, RakNet::Time timeStamp)
{
	RakNet::BitStream bs;

	// Writing the time stamp first allows raknet to convert local times between systems
	bs.Write((RakNet::MessageID)ID_TIMESTAMP);
	bs.Write(timeStamp);

	bs.Write((RakNet::MessageID)ID_SERVER_UPDATE_GAME_OBJECT);
	bs.Write(object->getID());
	bs.Write(object->position);
	bs.Write(object->rotation);
	bs.Write(object->getVelocity());
	bs.Write(object->getAngularVelocity());

	// Send the packet to all clients. It is not garenteed to arrive, but are sent often
	peerInterface->Send(&bs, MEDIUM_PRIORITY, UNRELIABLE, 1, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}
