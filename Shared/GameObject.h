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
	GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass, Collider* collider = nullptr);

	// Appends serialization data to bsInOut, used to create game objects on clients
	virtual void serialize(RakNet::BitStream& bsInOut) const override;


	// Apply a physics step to the object
	void physicsStep(float timeStep);

	// Apply a force at a point on the object. Affects both linear and angular velocities
	void applyForce(raylib::Vector3 force, raylib::Vector3 relitivePosition);
	// Resolve a collision with another object, applying appropriate forces to each object
	bool resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3 collisionNormal = raylib::Vector3(0));


	// Update this objects physics state, then extrapolate to the current time with optional smoothing
	void updateState(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, bool useSmoothing = false);
	// Apply a diff state to this object, then extrapolate to the current time with optional smoothing
	void applyStateDiff(const PhysicsState& diffState, RakNet::Time stateTime, RakNet::Time currentTime, bool useSmoothing = false, bool shouldUpdateObjectTime = false);


	unsigned int getID() const { return objectID; }

	raylib::Vector3 getVelocity() const { return velocity; }
	raylib::Vector3 getAngularVelocity() const { return angularVelocity; }
	void setVelocity(raylib::Vector3 vel) { velocity = vel; }
	void setAngularVelocity(raylib::Vector3 vel) { angularVelocity = vel; }

	float getMass() const { return mass; }
	raylib::Matrix getMoment() const { return moment; }
	float getElasticity() const { return elasticity; }


	RakNet::Time getTime() const { return lastPacketTime; }


protected:
	// Event called after a collison with 'other' is resolved
	virtual void onCollision(StaticObject* other) {};



protected:
	// How far away from the servers position we can be before being snapped to it when using smoothing
	const float smooth_snapDistance = 100;
	// How much to move toward the servers position when using smoothing
	const float smooth_moveFraction = 0.1f;

	// A unique identifier for this object, nessesary for sending updates over the network
	const unsigned int objectID;

	// The timestamp of the last packet receved for this object
	// Time in milliseconds. Multiply by 0.001 for seconds
	RakNet::Time lastPacketTime;


	// From StaticObject:
	//Vector3 position
	//Vector3 rotation
	
	raylib::Vector3 velocity;
	raylib::Vector3 angularVelocity;

	float mass;
	raylib::Matrix moment;
	float elasticity;
};