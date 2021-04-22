#pragma once
#include "raylib-cpp.hpp"
#include "Collider.h"
#include <BitStream.h>

class StaticObject
{
public:
	StaticObject(raylib::Vector3 position, raylib::Vector3 rotation);
	virtual ~StaticObject();

	// Appends serialization data to bsInOut, used by the server to send object creation packets
	virtual void serialize(RakNet::BitStream& bsInOut) const;

	//there needs to be some way for the server to know thew size of a serialized bitstream, because a 
	//size value needs to be at the begining of each static object, since the package contains multiple objects.
	//if its possible to set the writing position in a bitstream and insert data without overwritting existing 
	//data, this is not a problem, since it could use the delta position to determine the size.


	raylib::Vector3 position;
	raylib::Vector3 rotation;

	// Collider is only used by server for collision detection
	Collider* collider;


protected:
	// An identifier used for factory methods
	unsigned int typeID = 0;


protected:
	// Derived class GameObject sets this to false. Used for physics
	bool bIsStatic = true;
public:
	bool isStatic() const { return bIsStatic; }

};