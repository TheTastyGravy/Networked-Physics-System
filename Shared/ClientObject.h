#pragma once
#include "GameObject.h"

class ClientObject : public GameObject
{
public:
	ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass);


	struct PhysicsState
	{
		raylib::Vector3 position;
		raylib::Vector3 rotation;

		raylib::Vector3 velocity;
		raylib::Vector3 angularVelocity;
	};

	virtual PhysicsState processInputMovement(const RakNet::BitStream& inputData) = 0;
	virtual void processInputAction(const RakNet::BitStream& inputData) = 0;

};