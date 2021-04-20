#include "Server.h"
#include "../Shared/GameMessages.h"


void Server::destroyObject(unsigned int objectID)
{
	// Packet only contains objectID to destroy
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)ID_SERVER_DESTROY_GAME_OBJECT);
	bs.Write(objectID);
	// Send packet to all clients, reliably
	peerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);


	//if object id is server object, destroy from gameObjects
	//else destroy from clientObjects
}



void Server::applyPhysicsStep(float timeStep)
{
	// Update game objects and client objects
	for (auto& it : gameObjects)
	{
		it.second->physicsStep(timeStep);
	}

	for (auto& it : clientObjects)
	{
		it.second->physicsStep(timeStep);
	}
}
