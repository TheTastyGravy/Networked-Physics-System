#include "GameObject.h"
#include <GetTime.h>


GameObject::GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass) :
	StaticObject(position, rotation), objectID(objectID), lastPacketTime(0),
	mass(mass), velocity(raylib::Vector3(0)), angularVelocity(raylib::Vector3(0))
{}


void GameObject::serialize(RakNet::BitStream& bs) const
{
	bs.Write(RakNet::GetTime());
	bs.Write(objectID);
	
	// [typeID, position, rotation]
	StaticObject::serialize(bs);

	bs.Write(mass);
	bs.Write(velocity);
	bs.Write(angularVelocity);
}


void GameObject::physicsStep(float timeStep)
{
	position += velocity * timeStep;
	rotation += angularVelocity * timeStep;
}
