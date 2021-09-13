#pragma once
#include "GameObject.h"
#include <functional>

// Forward declaration
template<class T>
class RingBuffer;

/// <summary>
/// Contains player input processed by client objects
/// </summary>
struct Input
{
	Input() :
		movement(Vector2Zero()), mouseDelta(Vector2Zero()), mousePos(Vector2Zero()), jump(false), fire(false)
	{}


	raylib::Vector2 movement, mouseDelta, mousePos;
	bool jump, fire;
};

/// <summary>
/// A game object that is owned and can be controlled by a client
/// </summary>
class ClientObject : public GameObject
{
public:
	ClientObject();
	ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass, float elasticity, Collider* collider = nullptr, float linearDrag = 0, float angularDrag = 0, float friction = 1, bool lockRotation = false);
	ClientObject(PhysicsState initState, unsigned int clientID, float mass, float elasticity, Collider* collider = nullptr, float linearDrag = 0, float angularDrag = 0, float friction = 1, bool lockRotation = false);


	// Appends serialization data to bsInOut, used to create game and client objects on clients
	virtual void serialize(RakNet::BitStream& bsInOut) const
	{
		GameObject::serialize(bsInOut);
	}


	// Returns a diference in physics state
	virtual PhysicsState processInputMovement(const Input& input) const = 0;
	// Process actions, i.e. things that do not cause movement
	virtual void processInputAction(const Input& input, RakNet::Time timeStamp) = 0;


	// Used internally by client to apply a server update, then reapply input predictions
	void updateStateWithInputBuffer(const PhysicsState& state, const unsigned int stateFrame, const RakNet::Time timeBetweenFrames, const RingBuffer<std::tuple<unsigned int, PhysicsState, Input, uint32_t>>& inputBuffer, bool useSmoothing = false, std::function<void()> collisionCheck = [](){});
};