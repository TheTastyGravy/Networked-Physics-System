#pragma once
#include "GameObject.h"

// Forward declaration
template<class T>
class RingBuffer;

// Contains player input processed by ClientObjects
struct Input
{
	bool b1, b2, b3, b4, b5, b6, b7, b8;
	float f1, f2, f3, f4, f5, f6, f7, f8;
	raylib::Vector3 v1, v2, v3, v4, v5, v6, v7, v8;
};

class ClientObject : public GameObject
{
public:
	ClientObject();
	ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass, float elasticity, Collider* collider = nullptr);
	ClientObject(PhysicsState initState, unsigned int clientID, float mass, float elasticity, Collider* collider = nullptr);


	// Note: serialize() is used for transmitting both GameObjects and ClientObjects, and a ClientObject can be used as a GameObject.

	// Appends serialization data to bsInOut, used to create game objects on clients
	virtual void serialize(RakNet::BitStream& bsInOut) const
	{
		GameObject::serialize(bsInOut);
	}


	// Returns a diference in physics state
	virtual PhysicsState processInputMovement(const Input& input) const
	{
		//	---	basic movement for testing	---

		PhysicsState state;
		state.position = raylib::Vector3(0);
		state.rotation = raylib::Vector3(0);
		state.angularVelocity = raylib::Vector3(0);

		//get velocity vector
		raylib::Vector3 vel(0);
		if (input.b1)
			vel.y += 1;
		if (input.b3)
			vel.y -= 1;
		if (input.b2)
			vel.x -= 1;
		if (input.b4)
			vel.x += 1;

		//get velocity as a difference
		state.velocity = vel.Normalize() * 20;
		state.velocity -= velocity;

		return state;
	}
	virtual void processInputAction(const Input& input, RakNet::Time timeStamp) {};


	// Similar to updateState, but uses an input buffer to get to the current time
	void updateStateWithInputBuffer(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, const RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input>>& inputBuffer, bool useSmoothing = false);
};