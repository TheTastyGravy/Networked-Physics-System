#pragma once
#include "raylib-cpp.hpp"
#include "Collider.h"
#include <BitStream.h>


class StaticObject
{
public:
	StaticObject();
	StaticObject(raylib::Vector3 position, raylib::Vector3 rotation, Collider* collider = nullptr);
	virtual ~StaticObject();

	// Appends serialization data to bsInOut, used to create objects on clients
	virtual void serialize(RakNet::BitStream& bsInOut) const;


	bool isStatic() const { return bIsStatic; }

	Collider* getCollider() const { return collider; }



	raylib::Vector3 position;
	raylib::Vector3 rotation;
	
protected:
	// An identifier used for factory methods.
	// Every custom class needs to set this to a unique value
	int typeID = 0;

	// Derived class GameObject sets this to false. Used for physics
	bool bIsStatic = true;

	// Collider used for collision
	Collider* collider;
};