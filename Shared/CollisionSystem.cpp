#include "CollisionSystem.h"
#include "Sphere.h"
#include "OBB.h"

#include <iostream>


struct Interval
{
	float min, max;
};

struct Line
{
	raylib::Vector3 start, end;
};

struct Plane
{
	raylib::Vector3 normal;
	float distance;
};


std::vector<raylib::Vector3> getVertices(const StaticObject& obb)
{
	raylib::Vector3 center = obb.position;
	raylib::Vector3 extents = ((OBB*)obb.getCollider())->getHalfExtents();
	raylib::Matrix rot = MatrixRotateXYZ(obb.rotation);
	// Determine the rotated axes of the box
	raylib::Vector3 axes[] =
	{
		raylib::Vector3(rot.m0, rot.m1, rot.m2),
		raylib::Vector3(rot.m4, rot.m5, rot.m6),
		raylib::Vector3(rot.m8, rot.m9, rot.m10),
	};


	std::vector<raylib::Vector3> result;
	result.resize(8);

	// Calculate the 8 corners
	result[0] = center + axes[0] * extents.x + axes[1] * extents.y + axes[2] * extents.z;
	result[1] = center - axes[0] * extents.x + axes[1] * extents.y + axes[2] * extents.z;
	result[2] = center + axes[0] * extents.x - axes[1] * extents.y + axes[2] * extents.z;
	result[3] = center + axes[0] * extents.x + axes[1] * extents.y - axes[2] * extents.z;
	result[4] = center - axes[0] * extents.x - axes[1] * extents.y - axes[2] * extents.z;
	result[5] = center + axes[0] * extents.x - axes[1] * extents.y - axes[2] * extents.z;
	result[6] = center - axes[0] * extents.x + axes[1] * extents.y - axes[2] * extents.z;
	result[7] = center - axes[0] * extents.x - axes[1] * extents.y + axes[2] * extents.z;

	return result;
}

std::vector<Line> getEdges(const StaticObject& obb)
{
	std::vector<raylib::Vector3> corners = getVertices(obb);

	std::vector<Line> result;
	result.reserve(12);

	// Each edge is two connected corners
	result.push_back({ corners[6], corners[1] });
	result.push_back({ corners[6], corners[3] });
	result.push_back({ corners[6], corners[4] });
	result.push_back({ corners[2], corners[7] });
	result.push_back({ corners[2], corners[5] });
	result.push_back({ corners[2], corners[0] });
	result.push_back({ corners[0], corners[1] });
	result.push_back({ corners[0], corners[3] });
	result.push_back({ corners[7], corners[1] });
	result.push_back({ corners[7], corners[4] });
	result.push_back({ corners[4], corners[5] });
	result.push_back({ corners[5], corners[3] });

	return result;
}


Interval getInterval(const StaticObject& obb, const raylib::Vector3& axis)
{
	// Get each corner of the box
	std::vector<raylib::Vector3> vertex = getVertices(obb);

	Interval result;
	result.min = INFINITY;
	result.max = -INFINITY;

	// Project each corner onto the axis to find the min and max lengths
	for (int i = 0; i < 8; i++)
	{
		float projection = Vector3DotProduct(axis, vertex[i]);
		result.min = (projection < result.min) ? projection : result.min;
		result.max = (projection > result.max) ? projection : result.max;
	}

	return result;
}

float penetrationDepth(const StaticObject& obb1, const StaticObject& obb2, const raylib::Vector3& axis, bool& outShouldFlip)
{
	// Project both boxes along the axis
	Interval i1 = getInterval(obb1, Vector3Normalize(axis));
	Interval i2 = getInterval(obb2, Vector3Normalize(axis));

	// There is no overlap
	if (!((i2.min <= i1.max) && (i1.min <= i2.max)))
	{
		return 0.0f;
	}

	// Find the length of each projection
	float len1 = i1.max - i1.min;
	float len2 = i2.max - i2.min;
	// Find the length of both projections combined
	float min = fminf(i1.min, i2.min);
	float max = fmaxf(i1.max, i2.max);
	float combinedLen = max - min;


	outShouldFlip = (i2.min < i1.min);
	
	// Overlap = total length - combined length
	return (len1 + len2) - combinedLen;
}


std::vector<raylib::Vector3> clipEdgesToOBB(const std::vector<Line>& edges, const StaticObject& obb)
{
	raylib::Vector3 obbPosition = obb.position;
	raylib::Matrix rot = MatrixRotateXYZ(obb.rotation);
	// Determine the rotated axes of the box
	raylib::Vector3 axes[] =
	{
		raylib::Vector3(rot.m0, rot.m1, rot.m2),
		raylib::Vector3(rot.m4, rot.m5, rot.m6),
		raylib::Vector3(rot.m8, rot.m9, rot.m10),
	};
	// Get extents as an array
	raylib::Vector3 extents = ((OBB*)obb.getCollider())->getHalfExtents();
	float extentsArr[] = { extents.x, extents.y, extents.z };

	// Determine the 6 faces of the obb
	std::vector<Plane> planes;
	planes.resize(6);
	planes[0] = { axes[0], Vector3DotProduct(axes[0], (obbPosition + axes[0] * extents.x)) };
	planes[1] = { axes[0] * -1.0f, -Vector3DotProduct(axes[0], (obbPosition - axes[0] * extents.x)) };
	planes[2] = { axes[1], Vector3DotProduct(axes[1], (obbPosition + axes[1] * extents.y)) };
	planes[3] = { axes[1] * -1.0f, -Vector3DotProduct(axes[1], (obbPosition - axes[1] * extents.y)) };
	planes[4] = { axes[2], Vector3DotProduct(axes[2], (obbPosition + axes[2] * extents.z)) };
	planes[5] = { axes[2] * -1.0f, -Vector3DotProduct(axes[2], (obbPosition - axes[2] * extents.z)) };
	

	std::vector<raylib::Vector3> result;
	result.reserve(edges.size() * 3);

	// Clip every edge to every plane, and if they intersect, keep it
	for (int i = 0; i < planes.size(); i++)
	{
		for (int j = 0; j < edges.size(); j++)
		{
			raylib::Vector3 ab = Vector3Subtract(edges[j].end, edges[j].start);

			float nA = Vector3DotProduct(planes[i].normal, edges[j].start);
			float nAB = Vector3DotProduct(planes[i].normal, ab);

			if (nAB == 0)
			{
				continue;
			}

			float t = (planes[i].distance - nA) / nAB;
			if (t >= 0 && t <= 1)
			{
				// Find the point where the edge intersects the plane, and get it in local space
				raylib::Vector3 intersection = Vector3Add(edges[j].start, ab * t);
				raylib::Vector3 localPos = intersection - obbPosition;

				bool isInside = true;
				for (int i = 0; i < 3; i++)
				{
					// Find the distance along each axis, and if its larger than the extents, its outside
					float distance = Vector3DotProduct(localPos, axes[i]);
					if (distance > extentsArr[i] || distance < -extentsArr[i])
					{
						isInside = false;
						break;
					}
				}

				// The point is within all extents, so its inside the box
				if (isInside)
				{
					result.push_back(intersection);
				}
			}
		}
	}

	return result;
}



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
	// Set default values for output
	collisionPointOut = Vector3Zero();
	penOut = INFINITY;

	raylib::Matrix rot1 = MatrixRotateXYZ(obj1->rotation);
	raylib::Matrix rot2 = MatrixRotateXYZ(obj2->rotation);

	// The axes to test for seperation
	raylib::Vector3 test[15] = 
	{
		raylib::Vector3(rot1.m0, rot1.m1, rot1.m2),
		raylib::Vector3(rot1.m4, rot1.m5, rot1.m6),
		raylib::Vector3(rot1.m8, rot1.m9, rot1.m10),
		raylib::Vector3(rot2.m0, rot2.m1, rot2.m2),
		raylib::Vector3(rot2.m4, rot2.m5, rot2.m6),
		raylib::Vector3(rot2.m8, rot2.m9, rot2.m10)
	};
	// Edge axes are cross products between each face
	for (int i = 0; i < 3; i++)
	{
		test[6 + i * 3 + 0] = Vector3CrossProduct(test[i], test[0]);
		test[6 + i * 3 + 1] = Vector3CrossProduct(test[i], test[1]);
		test[6 + i * 3 + 2] = Vector3CrossProduct(test[i], test[2]);
	}


	bool shouldFlip;
	
	for (int i = 0; i < 15; i++)
	{
		// If a test axis is close to 0, skip it
		if (test[i].x < 0.000001f) test[i].x = 0;
		if (test[i].y < 0.000001f) test[i].y = 0;
		if (test[i].z < 0.000001f) test[i].z = 0;
		if (Vector3Length(test[i]) < 0.001f)
		{
			continue;
		}


		// Get the penetration along the axis
		float pen = penetrationDepth(*obj1, *obj2, test[i], shouldFlip);
		// Found a seperating axis, exit
		if (pen <= 0)
		{
			return false;
		}

		// Keep track of the axis of min penetration
		else if (pen < penOut)
		{
			if (shouldFlip)
			{
				test[i] = -test[i];
			}
			penOut = pen;
			collisionNormalOut = test[i];
		}
	}


	raylib::Vector3 axis = collisionNormalOut.Normalize();

	// Get contact points
	std::vector<raylib::Vector3> contacts;
	{
		// Clip both boxes to each other to find contact points
		std::vector<raylib::Vector3> c1 = clipEdgesToOBB(getEdges(*obj2), *obj1);
		std::vector<raylib::Vector3> c2 = clipEdgesToOBB(getEdges(*obj1), *obj2);
		// Add all contact points to a single vector
		contacts.reserve(c1.size() + c2.size());
		contacts.insert(contacts.end(), c1.begin(), c1.end());
		contacts.insert(contacts.end(), c2.begin(), c2.end());
	}

	Interval i = getInterval(*obj1, axis);
	float distance = (i.max - i.min) * 0.5f - penOut * 0.5f;
	raylib::Vector3 pointOnPlane = obj1->position + axis * distance;

	for (int i = 0; i < contacts.size(); i++)
	{
		raylib::Vector3 contact = contacts[i];
		contacts[i] = contact + (axis * Vector3DotProduct(axis, pointOnPlane - contact));
	}


	
	if (contacts.size() > 0)
	{
		// Use the average contact
		for (int i = 0; i < contacts.size(); i++)
		{
			collisionPointOut += contacts[i];
		}
		collisionPointOut /= contacts.size();
	}
	else
	{
		// If there are no contacts, use the point between the boxes
		collisionPointOut = (obj1->position + obj2->position) * 0.5f;
	}
	
	collisionNormalOut = axis;


	std::cout << "Pen: " << penOut << std::endl <<
		"Normal: " << collisionNormalOut.x << " " << collisionNormalOut.y << " " << collisionNormalOut.z << std::endl <<
		"Contact: " << collisionPointOut.x << " " << collisionPointOut.y << " " << collisionPointOut.z << std::endl << std::endl;

	return true;
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
	// Check if there is a collision, and resolve it if so
	if (colFuncPtr(object1, object2, normal, contact, pen))
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