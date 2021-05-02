#include "CollisionSystem.h"
#include "Sphere.h"
#include "OBB.h"


bool Sphere2Sphere(StaticObject* obj1, StaticObject* obj2, raylib::Vector3& collisionNormalOut, raylib::Vector3& collisionPointOut, float& penOut)
{
	float radius1 = static_cast<Sphere*>(obj1->getCollider())->getRadius();
	float radius2 = static_cast<Sphere*>(obj2->getCollider())->getRadius();

	float dist = Vector3Distance(obj1->position, obj2->position);
	penOut = (radius1 + radius2) - dist;

	// If the penetration is positive, a collision has occured
	if (penOut > 0)
	{
		// Find the collision normal relitive to obj1
		collisionNormalOut = Vector3Normalize(obj2->position - obj1->position);
		collisionPointOut = (obj1->position + obj2->position) * 0.5f;

		return true;
	}

	return false;
}
bool Sphere2Box(StaticObject* obj1, StaticObject* obj2, raylib::Vector3& collisionNormalOut, raylib::Vector3& collisionPointOut, float& penOut)
{
	float radius = static_cast<Sphere*>(obj1->getCollider())->getRadius();
	raylib::Vector3 extents = static_cast<OBB*>(obj2->getCollider())->getHalfExtents();


	// Get the spheres position in the boxes local space
	raylib::Vector3 localPos = obj1->position - obj2->position;
	localPos = localPos.Transform(MatrixRotateXYZ(obj2->rotation));

	// Find the closest point on the box to the sphere
	raylib::Vector3 closestPoint = localPos;
	closestPoint.x = Clamp(closestPoint.x, -extents.x, extents.x);
	closestPoint.y = Clamp(closestPoint.y, -extents.y, extents.y);
	closestPoint.z = Clamp(closestPoint.z, -extents.z, extents.z);

	// Transform the closest point to world space
	closestPoint = closestPoint.Transform(MatrixRotateXYZ(-obj2->rotation));
	closestPoint += obj2->position;


	// Get the point relitive to the sphere
	raylib::Vector3 closestPointOnSphere = obj1->position - closestPoint;

	penOut = radius - closestPointOnSphere.Length();
	if (penOut > 0)
	{
		collisionNormalOut = -closestPointOnSphere.Normalize();
		collisionPointOut = closestPoint;
		return true;
	}

	return false;
}
bool Box2Sphere(StaticObject* obj1, StaticObject* obj2, raylib::Vector3& collisionNormalOut, raylib::Vector3& collisionPointOut, float& penOut)
{
	bool res = Sphere2Box(obj2, obj1, collisionNormalOut, collisionPointOut, penOut);
	// Reverse the normal
	collisionNormalOut = -collisionNormalOut;
	return res;
}
bool Box2Box(StaticObject* obj1, StaticObject* obj2, raylib::Vector3& collisionNormalOut, raylib::Vector3& collisionPointOut, float& penOut)
{
	return false;
}

// Function pointer array for handling our collisions
typedef bool(*colFunc)(StaticObject*, StaticObject*, raylib::Vector3&, raylib::Vector3&, float&);
colFunc collisionFunctionArray[] =
{
	Sphere2Sphere, Sphere2Box,
	Box2Sphere, Box2Box
};



void CollisionSystem::handleCollision(GameObject* object1, StaticObject* object2, bool isOnServer, bool shouldAffectObject2)
{
	// If one of the objects have no collider, dont check
	if (!object1->getCollider() || !object2->getCollider())
	{
		return;
	}

	int shapeID1 = object1->getCollider()->getShapeID();
	int shapeID2 = object2->getCollider()->getShapeID();

	// An ID below 0 is invalid
	if (shapeID1 < 0 || shapeID2 < 0)
	{
		return;
	}


	// Get the collision detection function
	int funcIndex = (shapeID1 * Collider::SHAPE_COUNT) + shapeID2;
	auto colFuncPtr = collisionFunctionArray[funcIndex];
	if (colFuncPtr == nullptr)
	{
		return;
	}


	raylib::Vector3 normal, contact;
	float pen;
	// Check if a collision has occured, and get collision data if so
	bool res = colFuncPtr(object1, object2, normal, contact, pen);

	// Handle the collision that occured
	if (res)
	{
		object1->resolveCollision(object2, contact, normal, isOnServer, shouldAffectObject2);
		applyContactForces(object1, shouldAffectObject2 ? object2 : nullptr, normal, pen);
	}
}

void CollisionSystem::applyContactForces(StaticObject* obj1, StaticObject* obj2, raylib::Vector3 collisionNorm, float pen)
{
	// Try to cast the objects to a game object
	GameObject* gameObj1 = nullptr;
	if (!obj1->isStatic())
	{
		gameObj1 = static_cast<GameObject*>(obj1);
	}
	GameObject* gameObj2 = nullptr;
	if (!obj2->isStatic())
	{
		gameObj2 = static_cast<GameObject*>(obj2);
	}


	// If no obj2 was passed, use 'infinite' mass
	float body2Mass = gameObj2 ? gameObj2->getMass() : INT_MAX;
	float body1Factor = body2Mass / ((gameObj1 ? gameObj1->getMass() : INT_MAX) + body2Mass);


	// Apply contact forces
	if (gameObj1)
	{
		gameObj1->position -= collisionNorm * pen * body1Factor;
	}
	if (gameObj2)
	{
		gameObj2->position += collisionNorm * pen * (1 - body1Factor);
	}
}