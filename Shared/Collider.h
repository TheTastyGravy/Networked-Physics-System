#pragma once
#include "raylib-cpp.hpp"

class Collider
{
public:
	// Pure virtual so class is abstract
	virtual ~Collider() = 0;


protected:
	// Used to determine what derived class this collider is
	int shapeID;
public:
	int getShapeID() const { return shapeID; }


	//derived classes have collision info
};