#include "ClientObject.h"


ClientObject::ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass, Collider* collider) :
	GameObject(position, rotation, clientID, mass, collider)
{
	typeID = -2;
}


void ClientObject::updateStateWithInputBuffer(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, const RingBuffer<std::pair<RakNet::Time, Input>>& inputBuffer, bool useSmoothing)
{
	// If we are more up to date than this packet, ignore it
	if (stateTime < lastPacketTime)
	{
		return;
	}


	// Store our current state
	PhysicsState currentState;
	currentState.position = position;
	currentState.rotation = rotation;
	currentState.velocity = velocity;
	currentState.angularVelocity = angularVelocity;
	// Apply the new state
	position = state.position;
	rotation = state.rotation;
	velocity = state.velocity;
	angularVelocity = state.angularVelocity;


	RakNet::Time lastTime = stateTime;
	for (int i = 0; i < inputBuffer.getSize(); i++)
	{ 
		// The buffer contains a std::pair containing the input and its time
		auto& input = inputBuffer[i];

		// Ignore older inputs
		if (input.first < lastTime)
		{
			continue;
		}


		// Step forward to the time of the input
		float deltaTime = (input.first - lastTime) * 0.001f;
		physicsStep(deltaTime);

		// Process and apply the input
		PhysicsState diff = processInputMovement(input.second);
		position += diff.position;
		rotation += diff.rotation;
		velocity += diff.velocity;
		angularVelocity += diff.angularVelocity;

		lastTime = input.first;
	}


	// Step forward to the current time
	float deltaTime = (currentTime - lastTime) * 0.001f;
	physicsStep(deltaTime);
	

	// Should the position be updated with smoothing?
	if (useSmoothing)
	{
		float dist = Vector3Distance(currentState.position, position);

		if (dist < smooth_snapDistance && dist > 0.1f)
		{
			// Move some of the way to the new position
			position += (position - currentState.position) * smooth_moveFraction;
		}
	}


	// Update the absolute time for this object
	lastPacketTime = stateTime;
}
