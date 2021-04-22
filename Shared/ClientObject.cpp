#include "ClientObject.h"

ClientObject::ClientObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int clientID, float mass) :
	GameObject(position, rotation, mass)
{
	objectID = clientID;
}
