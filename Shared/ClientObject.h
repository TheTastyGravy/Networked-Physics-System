#pragma once
#include "GameObject.h"

struct PhysicsState
{
	raylib::Vector3 position;
	raylib::Vector3 rotation;

	raylib::Vector3 velocity;
	raylib::Vector3 angularVelocity;
};

class ClientObject : public GameObject
{
public:
	ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass);


	virtual PhysicsState processInputMovement(RakNet::BitStream& bsIn) = 0;
	virtual void processInputAction(RakNet::BitStream& bsIn) = 0;
};