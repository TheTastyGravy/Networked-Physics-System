#pragma once
#include "Collider.h"

class OBB : public Collider
{
public:
	OBB(raylib::Vector3 halfExtents) : halfExtents(halfExtents)
	{
		shapeID = 1;
	}


	raylib::Matrix calculateInertiaTensor(float mass) const
	{
		float val = (1.f / 12.f) * mass;

		// Multiply by 4 to convert half extent to full length
		float x = val * halfExtents.x * halfExtents.x * 4;
		float y = val * halfExtents.y * halfExtents.y * 4;
		float z = val * halfExtents.z * halfExtents.z * 4;

		return {
			y + z, 0, 0, 0,
			0, x + z, 0, 0,
			0, 0, x + y, 0,
			0, 0, 0, 1
		};
	}

	raylib::Vector3 getHalfExtents() const { return halfExtents; }

protected:
	virtual void serialize(RakNet::BitStream& bsIn) const
	{
		bsIn.Write(shapeID);
		bsIn.Write(halfExtents);
	}


private:
	raylib::Vector3 halfExtents;

};