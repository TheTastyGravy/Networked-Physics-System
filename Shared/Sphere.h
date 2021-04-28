#pragma once
#include "Collider.h"

class Sphere : public Collider
{
public:
	Sphere(float radius) : radius(radius)
	{
		shapeID = 0;
	}


	float getRadius() const { return radius; }


private:
	float radius;

};