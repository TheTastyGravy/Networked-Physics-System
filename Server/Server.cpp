#include "Server.h"
#include "../Shared/GameMessages.h"
#include <GetTime.h>



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



void Server::processSystemMessage(const RakNet::Packet* packet)
{
	// Check package ID to determine what it is
	switch (packet->data[0])
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
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(1);
		unsigned int id;
		bsIn.Read(id);
		processInput(id, &bsIn);
		break;
	}


	default:
		break;
	}
}




void Server::physicsUpdate(float deltaTime)
{
	//could implement fixed time step, but for now leave it

	collisionDetectionAndResolution();


	// Update game objects and client objects
	for (auto& it : gameObjects)
	{
		it.second->physicsStep(deltaTime);
	}
	for (auto& it : clientObjects)
	{
		it.second->physicsStep(deltaTime);
	}

	// Update current time
	currentTime = RakNet::GetTime();
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
	// Add client to map
	addressToClientID[RakNet::SystemAddress::ToInteger(connectedAddress)] = nextClientID;


	//send client id and static objects
	{
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_SERVER_SET_CLIENT_ID);
		bs.Write(nextClientID);
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

	// Send message to all clients to destroy the disconnected clients object
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_DESTROY_GAME_OBJECT);
	bs.Write(id);
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

	// Remove the client object and its address from the map
	clientObjects.erase(id);
	addressToClientID.erase(RakNet::SystemAddress::ToInteger(disconnectedAddress));
}


void Server::processInput(unsigned int clientID, RakNet::BitStream* bsIn)
{
	RakNet::Time timeStamp;
	bsIn->Read(timeStamp);

	ClientObject* clientObject = clientObjects[clientID];


	//this has problems...

	//only process movement if the packet is more recent than the last one receved
	if (timeStamp > clientObject->getTime())
	{
		PhysicsState state = clientObject->processInputMovement(*bsIn);

		//apply state
	}

	//always process actions
	clientObject->processInputAction(*bsIn);

	//update object time
	clientObject->setTime(timeStamp);
}
