#pragma once
#include "raylib-cpp.hpp"

class Collider
{
public:



protected:
	// Used to determine what derived class this collider is
	int shapeID = -1;
public:
	int getShapeID() const { return shapeID; }

	// The total number of shape IDs in use
	static const int SHAPE_COUNT = 2;


	//derived classes have collision info
};