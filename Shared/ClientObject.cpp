#include "ClientObject.h"
#include "RingBuffer.h"
#include <tuple>


ClientObject::ClientObject() :
	GameObject()
{
	typeID = -2;
}

ClientObject::ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass, float elasticity, Collider* collider) :
	GameObject(position, rotation, clientID, mass, elasticity, collider)
{
	typeID = -2;
}

ClientObject::ClientObject(PhysicsState initState, unsigned int clientID, float mass, float elasticity, Collider* collider) :
	GameObject(initState, clientID, mass, elasticity, collider)
{
	typeID = -2;
}


void ClientObject::updateStateWithInputBuffer(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, 
											  const RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input>>& inputBuffer, bool useSmoothing)
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
	bool isFirstInput = true;
	for (int i = 0; i < inputBuffer.getSize(); i++)
	{ 
		// The buffer contains a std::tuple containing time, physics state, and input
		auto& input = inputBuffer[i];

		RakNet::Time inputTime = std::get<0>(input);
		// Ignore older inputs
		if (inputTime < lastTime)
		{
			continue;
		}

		// Step forward to the time of the input
		float deltaTime = (inputTime - lastTime) * 0.001f;
		physicsStep(deltaTime);


		// For the first input we process, check the difference in state. If they are 
		// close, use the predicted state
		if (isFirstInput)
		{
			PhysicsState inputState = std::get<1>(input);
			if (Vector3Distance(position, inputState.position) < smooth_threshold &&
				Vector3Distance(rotation, inputState.rotation) < smooth_threshold &&
				Vector3Distance(velocity, inputState.velocity) < smooth_threshold)
			{
				position = currentState.position;
				rotation = currentState.rotation;
				velocity = currentState.velocity;
				angularVelocity = currentState.angularVelocity;

				// Update the absolute time for this object
				lastPacketTime = stateTime;
				return;
			}

			isFirstInput = false;
		}
		
		
		// Process and apply the input
		PhysicsState diff = processInputMovement(std::get<2>(input));
		position += diff.position;
		rotation += diff.rotation;
		velocity += diff.velocity;
		angularVelocity += diff.angularVelocity;
		

		lastTime = inputTime;
	}

	// Step forward to the current time
	float deltaTime = (currentTime - lastTime) * 0.001f;
	physicsStep(deltaTime);
	

	// Should the position be updated with smoothing?
	if (useSmoothing)
	{
		float dist = Vector3Distance(currentState.position, position);

		if (dist < smooth_snapDistance && dist > smooth_threshold)
		{
			// Move some of the way to the new position
			position = currentState.position + (position - currentState.position) * smooth_moveFraction;
		}
		else if (dist < smooth_threshold)	// We are close, use predicted value
		{
			position = currentState.position;
		}
	}


	// Update the absolute time for this object
	lastPacketTime = stateTime;
}
