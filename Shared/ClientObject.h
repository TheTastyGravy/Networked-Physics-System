#pragma once
#include "GameObject.h"

// Forward declaration
template<class T>
class RingBuffer;

// Contains player input processed by ClientObjects
struct Input
{
	Input() :
		movement(Vector2Zero()), mouseDelta(Vector2Zero()), mousePos(Vector2Zero()), jump(false), fire(false), 
		bool1(false), bool2(false), bool3(false), bool4(false), bool5(false), bool6(false), bool7(false), bool8(false), 
		float1(0), float2(0), float3(0), float4(0), 
		vec1(Vector3Zero()), vec2(Vector3Zero()), vec3(Vector3Zero()), vec4(Vector3Zero())
	{}


	// Common inputs most games are likely to use

	raylib::Vector2 movement, mouseDelta, mousePos;
	bool jump, fire;

	// As a generic system, input the user may need is unknowable, so give the user some common value types

	bool bool1, bool2, bool3, bool4, bool5, bool6, bool7, bool8;
	float float1, float2, float3, float4;
	raylib::Vector3 vec1, vec2, vec3, vec4;
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
		if (input.bool1)
			vel.y += 1;
		if (input.bool3)
			vel.y -= 1;
		if (input.bool2)
			vel.x -= 1;
		if (input.bool4)
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