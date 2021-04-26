#include "GameObject.h"


GameObject::GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass) :
	StaticObject(position, rotation), objectID(objectID), lastPacketTime(0),
	mass(mass), velocity(raylib::Vector3(0)), angularVelocity(raylib::Vector3(0))
{}


void GameObject::serialize(RakNet::BitStream& bs) const
{
	bs.Write(objectID);
	
	// [typeID, position, rotation]
	StaticObject::serialize(bs);

	bs.Write(velocity);
	bs.Write(angularVelocity);
}


void GameObject::physicsStep(float timeStep)
{
	position += velocity * timeStep;
	rotation += angularVelocity * timeStep;
}


void GameObject::applyForce(raylib::Vector3 force, raylib::Vector3 relitivePosition)
{
	velocity += force / getMass();
	angularVelocity += force.CrossProduct(relitivePosition) / getMoment();
}


//	----------   THIS FUNCTION IS ALMOST CERTAINLY BROKEN   ----------
void GameObject::resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3 collisionNormal, float pen)
{
	//this code is an eye sore with all of the checks to determine if the other object is a game object.
	//adding getters for mass, velocity, etc to StaticObject returning the default values used, and overriding 
	//them for GameObject would reduce this bulk

	if (otherObject == nullptr)
	{
		return;
	}

	// Find the vector between their centers, or use the provided
	// direction of force, and make sure its normalized
	raylib::Vector3 normal = Vector3Normalize(collisionNormal == raylib::Vector3(0) ? otherObject->position - position : collisionNormal);


	// If the other object is not static, cast it to a game object
	GameObject* otherGameObj = otherObject->isStatic() ? nullptr : static_cast<GameObject*>(otherObject);



	// Find the velocities of the contact points
	//possible point of error: angular velocity may need to be converted to change in orientation
	raylib::Vector3 pointVel1 = velocity + angularVelocity.CrossProduct(contact - position);
	raylib::Vector3 pointVel2 = (otherGameObj ? otherGameObj->getVelocity() : raylib::Vector3(0)) + 
					(otherGameObj ? otherGameObj->getAngularVelocity() : raylib::Vector3(0)).CrossProduct(contact - otherObject->position);

	// Find the velocity of the points along the normal, ie toward eachother
	//this could be wrong... idk
	float normalVelocity1 = pointVel1.DotProduct(normal);
	float normalVelocity2 = pointVel2.DotProduct(normal);



	if (normalVelocity1 > normalVelocity2) // They are moving closer
	{
		// This has been written referencing multiple sources with conflicting methods. Consequently, the major 
		// differences have been commented, so when this is able to be tested (after collision detection is added), 
		// the areas likely to be causing problems can be determined. Good luck
		
		//the radius from the center to the collision point
		raylib::Vector3 radius1 = contact - position;
		raylib::Vector3 radius2 = contact - otherObject->position;

		//normal is probably not ment to be here
		raylib::Vector3 relitiveVelocity = normal * (pointVel1 - pointVel2);
		//the brackets might be wrong: elasticity * relitiveVelocity.dot(normal)
		float numerator = (relitiveVelocity * (1 + 0.5f * (getElasticity() + (otherGameObj ? otherGameObj->getElasticity() : 1)))).DotProduct(normal);

		//N dot (((r × N) * 1/I) × r)
		//the dot product may be the wrong way around
		float obj1Value = normal.DotProduct((radius1.CrossProduct(normal) * (1 / getMoment())).CrossProduct(radius1));
		float obj2Value = normal.DotProduct((radius2.CrossProduct(normal) * (1 / (otherGameObj ? otherGameObj->getMoment() : 0.0001f))).CrossProduct(radius2));

		float denominator = 1 / getMass() + 1 / (otherGameObj ? otherGameObj->getMass() : INFINITY) + obj1Value + obj2Value;

		float j = numerator / denominator;
		//the impulse is applied along the collision normal
		raylib::Vector3 impulse = normal * j;


		//	--- old method	---
		// This will calculate the effective mass at the point of each 
		// object (How much it will move due to the forces applied)
		//float mass1 = 1.0f / (1.0f / mass + (radius1 * radius1) / getMoment());
		//float mass2 = 1.0f / (1.0f / (otherGameObj ? otherGameObj->getMass() : INFINITY) + (radius2 * radius2) / (otherGameObj ? otherGameObj->getMoment() : 0.0001f));

		// Use the average elasticity of the colliding objects
		//float elasticity = 0.5f * (getElasticity() + (otherGameObj ? otherGameObj->getElasticity() : 1));

		//raylib::Vector3 impulse = normal * ((1.0f + elasticity) * mass1 * mass2 / (mass1 + mass2) * (normalVelocity1 - normalVelocity2));



		applyForce(impulse, radius1);
		if (otherGameObj)
		{
			otherGameObj->applyForce(-impulse, radius2);
		}

		// Trigger collision event
		onCollision(otherObject);
		if (otherGameObj)
		{
			otherGameObj->onCollision(this);
		}

		// Apply contact forces to prevent objects from being inside each other
		if (pen > 0)
		{
			//PhysicsScene::applyContactForces(this, otherGameObj, normal, pen);
		}
	}
}


void GameObject::updateState(const PhysicsState& state, RakNet::Time stateTime, RakNet::Time currentTime, bool useSmoothing)
{
	// If we are more up to date than this packet, ignore it
	if (stateTime < lastPacketTime)
	{
		return;
	}

	float deltaTime = (currentTime - stateTime) * 0.001f;

	PhysicsState newState(state);
	// Extrapolate to get the state at the current time using dead reckoning
	newState.position += newState.velocity * deltaTime;
	newState.rotation += newState.angularVelocity * deltaTime;


	// These values are less noticible when snapped, and can be set directly
	rotation = newState.rotation;
	velocity = newState.velocity;
	angularVelocity = newState.angularVelocity;

	// Should the position be updated with smoothing?
	if (useSmoothing)
	{
		float dist = Vector3Distance(newState.position, position);

		if (dist > smooth_snapDistance)
		{
			// We are too far away from the servers position: snap to it
			position = newState.position;
		}
		else if (dist > 0.1f) // If we are only a small distance from the real value, we dont move
		{
			// Move some of the way to the servers position
			position += (newState.position - position) * smooth_moveFraction;
		}
	}
	else
	{
		// Dont smooth: set directly
		position = newState.position;
	}

	
	// Update the absolute time for this object
	lastPacketTime = stateTime;
}

void GameObject::applyStateDiff(const PhysicsState& diffState, RakNet::Time stateTime, RakNet::Time currentTime, bool useSmoothing, bool shouldUpdateObjectTime)
{
	// If we are more up to date than this state, ignore it
	if (stateTime < lastPacketTime)
	{
		return;
	}


	// Add the diff to the current state
	raylib::Vector3 newPos = position + diffState.position;
	rotation += diffState.rotation;
	velocity += diffState.velocity;
	angularVelocity += diffState.angularVelocity;
	
	// Extrapolate to get the state at the current time using dead reckoning
	float deltaTime = (currentTime - stateTime) * 0.001f;
	newPos += velocity * deltaTime;
	rotation += angularVelocity * deltaTime;


	// Should the position be updated with smoothing?
	if (useSmoothing)
	{
		float dist = Vector3Distance(newPos, position);

		if (dist > smooth_snapDistance)
		{
			// We are too far away from the new position: snap to it
			position = newPos;
		}
		else if (dist > 0.1f) // If we are only a small distance from the new value, we dont move
		{
			// Move some of the way to the new position
			position += (newPos - position) * smooth_moveFraction;
		}
	}
	else
	{
		// Dont smooth: set directly
		position = newPos;
	}



	// If we should update the objects time, do so
	if (shouldUpdateObjectTime)
	{
		lastPacketTime = stateTime;
	}
}
