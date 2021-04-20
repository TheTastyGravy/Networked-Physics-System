#pragma once
#include "StaticObject.h"

class GameObject : public StaticObject
{
public:
	GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass);

	virtual void serialize(RakNet::BitStream& bsInOut) const override;


	// Apply a physics step to the object
	void physicsStep(float timeStep);

	void applyForce(raylib::Vector3 force, raylib::Vector3 position);
	void resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3* collisionNormal = nullptr, float pen = 0);


	//called every step that objects are colliding
	virtual void onCollision(StaticObject* other) {};
	

	unsigned int getID() const { return objectID; }

protected:
	unsigned int objectID;

	// Used by client and for client object. The timestamp of the last packet receved for this object
	RakNet::Time lastPacketTime;
	
	float mass;
	raylib::Vector3 velocity;
	raylib::Vector3 angularVelocity;
};