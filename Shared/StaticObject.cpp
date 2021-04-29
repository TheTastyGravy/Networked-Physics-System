#include "StaticObject.h"

StaticObject::StaticObject() :
	position(0,0,0), rotation(0,0,0), collider(nullptr)
{}

StaticObject::StaticObject(raylib::Vector3 position, raylib::Vector3 rotation, Collider* collider) :
	position(position), rotation(rotation), collider(collider)
{}

StaticObject::~StaticObject()
{
	if (collider != nullptr)
	{
		delete collider;
	}
}


void StaticObject::serialize(RakNet::BitStream& bs) const
{
	// [typeID, position, rotation]
	bs.Write(typeID);
	bs.Write(position);
	bs.Write(rotation);
}
