#include "StaticObject.h"

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
