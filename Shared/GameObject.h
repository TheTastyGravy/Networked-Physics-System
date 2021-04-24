#pragma once
#include "StaticObject.h"

struct PhysicsState
{
	raylib::Vector3 position;
	raylib::Vector3 rotation;

	raylib::Vector3 velocity;
	raylib::Vector3 angularVelocity;
};

class GameObject : public StaticObject
{
public:
	GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass);

	virtual void serialize(RakNet::BitStream& bsInOut) const override;


	// Apply a physics step to the object
	void physicsStep(float timeStep);

	void applyForce(raylib::Vector3 force, raylib::Vector3 relitivePosition);
	void resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3 collisionNormal = raylib::Vector3(0), float pen = 0);

	//abstract		called every step that objects are colliding
	virtual void onCollision(StaticObject* other) {};

	// Update this objects physics state, then extrapolate to the current time.
	// This is expected to be used by a client to apply updates from the server
	void updateState(const PhysicsState& state, RakNet::TimeMS stateTime, RakNet::TimeMS currentTime);

	// 
	void applyStateDif(const PhysicsState& stateDif, RakNet::TimeMS stateTime, RakNet::TimeMS currentTime);


	//function added for hack. allows server to get this state and add input dif
	PhysicsState getCurrentState() const { return { position, rotation, velocity, angularVelocity }; }

	

	unsigned int getID() const { return objectID; }

	raylib::Vector3 getVelocity() const { return velocity; }
	raylib::Vector3 getAngularVelocity() const { return angularVelocity; }
	void setVelocity(raylib::Vector3 vel) { velocity = vel; }
	void setAngularVelocity(raylib::Vector3 vel) { angularVelocity = vel; }

	float getMass() const { return mass; }
	float getMoment() const { return moment; }
	float getElasticity() const { return elasticity; }


	RakNet::TimeMS getTime() const { return lastPacketTime; }
	void setTime(RakNet::TimeMS time) { lastPacketTime = time; }


protected:
	const unsigned int objectID;

	// Used by client and for client object. The timestamp of the last packet receved for this object
	// Time in milliseconds. Multiply by 0.001 for seconds
	RakNet::TimeMS lastPacketTime;


	// From StaticObject:
	//Vector3 position
	//Vector3 rotation
	
	raylib::Vector3 velocity;
	raylib::Vector3 angularVelocity;

	float mass;
	float moment;
	float elasticity;
};