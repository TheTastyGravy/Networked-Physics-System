#pragma once
#include "StaticObject.h"

class GameObject : public StaticObject
{
public:
	GameObject(raylib::Vector3 position, raylib::Vector3 rotation, float mass);

	virtual void serialize(RakNet::BitStream& bsInOut) const override;


	// Apply a physics step to the object
	void physicsStep(float timeStep);

	void applyForce(raylib::Vector3 force, raylib::Vector3 position);
	void resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3 collisionNormal = raylib::Vector3(0), float pen = 0);


	//called every step that objects are colliding
	virtual void onCollision(StaticObject* other) {};
	

	unsigned int getID() const { return objectID; }

	raylib::Vector3 getVelocity() const { return velocity; }
	raylib::Vector3 getAngularVelocity() const { return angularVelocity; }
	float getMass() const { return mass; }
	float getMoment() const { return moment; }
	float getElasticity() const { return elasticity; }


	RakNet::Time getTime() const { return lastPacketTime; }
	void setTime(RakNet::Time time) { lastPacketTime = time; }


protected:
	unsigned int objectID;

	// Used by client and for client object. The timestamp of the last packet receved for this object
	RakNet::Time lastPacketTime;


	//Vector3 position
	//Vector3 rotation
	
	raylib::Vector3 velocity;
	raylib::Vector3 angularVelocity;

	float mass;
	float moment;
	float elasticity;
};