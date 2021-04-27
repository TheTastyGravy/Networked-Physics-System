#include "ClientObject.h"

#include <iostream>

ClientObject::ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass) :
	GameObject(position, rotation, clientID, mass)
{}


void ClientObject::updateStateWithInputBuffer(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, const RingBuffer<std::pair<RakNet::Time, PhysicsState>>& inputBuffer, bool useSmoothing)
{
	// If we are more up to date than this packet, ignore it
	if (stateTime < lastPacketTime)
	{
		return;
	}


	PhysicsState newState(state);

	RakNet::Time lastTime = stateTime;

	int count = 0;

	for (int i = 0; i < inputBuffer.getSize(); i++)
	{
		auto& input = inputBuffer[i];
	
		//only use input if its more recent
		if (input.first < lastTime)
		{
			count++;
			continue;
		}
	
	
		//get time diff
		float deltaTime = (input.first - lastTime) * 0.001f;
	
		//fast forward
		newState.position += newState.velocity * deltaTime;
		newState.rotation += newState.angularVelocity * deltaTime;
	
		//apply diff
		newState.position += input.second.position;
		newState.rotation += input.second.rotation;
		newState.velocity += input.second.velocity;
		newState.angularVelocity += input.second.angularVelocity;
	
		
		//update time
		lastTime = input.first;
	}

	std::cout << count << " inputs dropped" << std::endl;


	float deltaTime = (currentTime - lastTime) * 0.001f;

	// Extrapolate to get the state at the current time using dead reckoning
	newState.position += newState.velocity * deltaTime;
	newState.rotation += newState.angularVelocity * deltaTime;


	// These values are less noticible when snapped, and can be set directly
	rotation = newState.rotation;
	velocity = newState.velocity;
	angularVelocity = newState.angularVelocity;

	// Should the position be updated with smoothing?
	if (useSmoothing)
	{
		float dist = Vector3Distance(newState.position, position);

		if (dist > smooth_snapDistance)
		{
			// We are too far away from the servers position: snap to it
			position = newState.position;
		}
		else if (dist > 0.1f) // If we are only a small distance from the real value, we dont move
		{
			// Move some of the way to the servers position
			position += (newState.position - position) * smooth_moveFraction;
		}
	}
	else
	{
		// Dont smooth: set directly
		position = newState.position;
	}


	// Update the absolute time for this object
	lastPacketTime = stateTime;
}
