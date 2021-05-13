#pragma once
#include "GameObject.h"

class CollisionSystem
{
public:

	struct CallbackData
	{
		CallbackData() : 
			obj1(nullptr), obj2(nullptr), normal(Vector3Zero()), contact(Vector3Zero())
		{}

		GameObject* obj1;
		StaticObject* obj2;

		raylib::Vector3 normal;
		raylib::Vector3 contact;
	};


	// Check for and resolve a collision between two objects
	static void handleCollision(GameObject* object1, StaticObject* object2, bool shouldAffectObject2, CallbackData* callback = nullptr);

private:
	static void applyContactForces(StaticObject* obj1, StaticObject* obj2, raylib::Vector3 collisionNorm, float pen);
};