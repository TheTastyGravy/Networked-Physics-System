#pragma once
#include "raylib-cpp.hpp"

class Collider
{
public:

	// Calculate the moment of inertia tensor of an object with this colliders shape
	virtual raylib::Matrix calculateInertiaTensor(float mass) = 0;


protected:
	// Used to determine what derived class this collider is
	int shapeID = -1;
public:
	int getShapeID() const { return shapeID; }

	// The total number of shape IDs in use
	static const int SHAPE_COUNT = 2;
};