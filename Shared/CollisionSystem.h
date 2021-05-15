#pragma once
#include "GameObject.h"

class CollisionSystem
{
public:
	// Check for and resolve a collision between two objects
	static void handleCollision(GameObject* object1, StaticObject* object2, bool shouldAffectObject2);

private:
	static void applyContactForces(StaticObject* obj1, StaticObject* obj2, raylib::Vector3 collisionNorm, float pen);
};