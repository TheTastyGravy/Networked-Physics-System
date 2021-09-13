#include "ClientObject.h"
#include <tuple>
#include "RingBuffer.h"


ClientObject::ClientObject() :
	GameObject()
{
	typeID = -2;
}

ClientObject::ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass, float elasticity, Collider* collider, float linearDrag, float angularDrag, float friction, bool lockRotation) :
	GameObject(position, rotation, clientID, mass, elasticity, collider, linearDrag, angularDrag, friction, lockRotation)
{
	typeID = -2;
}

ClientObject::ClientObject(PhysicsState initState, unsigned int clientID, float mass, float elasticity, Collider* collider, float linearDrag, float angularDrag, float friction, bool lockRotation) :
	GameObject(initState, clientID, mass, elasticity, collider, linearDrag, angularDrag, friction, lockRotation)
{
	typeID = -2;
}


void ClientObject::updateStateWithInputBuffer(const PhysicsState& state, const unsigned int stateFrame, const RakNet::Time timeBetweenFrames, 
											  const RingBuffer<std::tuple<unsigned int, PhysicsState, Input, uint32_t>>& inputBuffer, bool useSmoothing, std::function<void()> collisionCheck)
{
	// If we are more up to date than this packet, ignore it
	if (stateFrame < lastPacketTime)
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


	unsigned int lastFrame = stateFrame;
	bool isFirstInput = true;
	for (unsigned short i = 0; i < inputBuffer.getSize(); i++)
	{ 
		// The buffer contains a std::tuple containing time, physics state, and input
		auto& input = inputBuffer[i];

		unsigned int inputFrame = std::get<0>(input);
		// Ignore older inputs
		if (inputFrame < lastFrame)
		{
			continue;
		}

		// Do collision detection with static and game objects
		collisionCheck();
		// Step forward to the time of the input
		float deltaTime = (inputFrame - lastFrame) * timeBetweenFrames * 0.001f;
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
				lastPacketTime = stateFrame;
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
		

		lastFrame = inputFrame;
	}

	// Step forward to the current time
	//float deltaTime = (currentTime - lastTime) * 0.001f;
	//physicsStep(deltaTime);
	

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
	lastPacketTime = stateFrame;
}
