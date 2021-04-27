#pragma once
#include "GameObject.h"
#include "RingBuffer.h"

class ClientObject : public GameObject
{
public:
	ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass);


	// Note: serialize() is used for transmitting both GameObjects and ClientObjects, and a ClientObject can be used as a GameObject.


	// Returns a diference in physics state
	virtual PhysicsState processInputMovement(RakNet::BitStream& bsIn) const
	{
		//	---	basic movement for testing	---

		PhysicsState state;
		state.position = raylib::Vector3(0);
		state.rotation = raylib::Vector3(0);
		state.angularVelocity = raylib::Vector3(0);

		//get key down states
		bool wDown, aDown, sDown, dDown;
		bsIn.Read(wDown);
		bsIn.Read(aDown);
		bsIn.Read(sDown);
		bsIn.Read(dDown);
		//get velocity vector
		raylib::Vector3 vel(0);
		if (wDown)
			vel.y += 1;
		if (sDown)
			vel.y -= 1;
		if (aDown)
			vel.x -= 1;
		if (dDown)
			vel.x += 1;

		//get velocity as a diference
		state.velocity = vel.Normalize() * 20;
		state.velocity -= velocity;

		return state;
	}
	virtual void processInputAction(RakNet::BitStream& bsIn, RakNet::Time timeStamp) {};


	// Similar to updateState, but uses an input buffer to get to the current time
	void updateStateWithInputBuffer(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, const RingBuffer<std::pair<RakNet::Time, PhysicsState>>& inputBuffer, bool useSmoothing = false);
};