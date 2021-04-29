#pragma once
#include "Collider.h"

class OBB : public Collider
{
public:
	OBB()
	{
		shapeID = 1;
	}


	raylib::Matrix calculateInertiaTensor(float mass)
	{

	}



private:



};