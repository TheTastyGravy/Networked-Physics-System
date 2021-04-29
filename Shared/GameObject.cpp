#include "GameObject.h"

#include <iostream>


GameObject::GameObject() :
	StaticObject(), objectID(-1), lastPacketTime(0), 
	velocity(0, 0, 0), angularVelocity(0, 0, 0), mass(1), elasticity(1)
{
	// Game objects are not static
	bIsStatic = false;
	typeID = -1;

	// Use the collider to get moment. If mass or the collider is ever changed, this should be recalculated
	moment = (getCollider() ? getCollider()->calculateInertiaTensor(mass) : MatrixIdentity());
}

GameObject::GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass, Collider* collider) :
	StaticObject(position, rotation, collider), objectID(objectID), lastPacketTime(0),
	velocity(0,0,0), angularVelocity(0,0,0), mass(mass), elasticity(1)
{
	// Game objects are not static
	bIsStatic = false;
	typeID = -1;

	// Use the collider to get moment. If mass or the collider is ever changed, this should be recalculated
	moment = (getCollider() ? getCollider()->calculateInertiaTensor(mass) : MatrixIdentity());
}

GameObject::GameObject(PhysicsState initState, unsigned int objectID, float mass, Collider* collider) :
	StaticObject(initState.position, initState.rotation, collider), objectID(objectID), lastPacketTime(0), 
	velocity(initState.velocity), angularVelocity(initState.angularVelocity), 
	mass(mass), elasticity(1)
{
	// Game objects are not static
	bIsStatic = false;
	typeID = -1;

	// Use the collider to get moment. If mass or the collider is ever changed, this should be recalculated
	moment = (getCollider() ? getCollider()->calculateInertiaTensor(mass) : MatrixIdentity());
}


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
	angularVelocity += force.CrossProduct(relitivePosition).Transform( getMoment().Invert() );
}

void GameObject::resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3 collisionNormal)
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
	raylib::Vector3 pointVel1 = velocity + angularVelocity.CrossProduct(contact - position);
	raylib::Vector3 pointVel2 = (otherGameObj ? otherGameObj->getVelocity() : raylib::Vector3(0)) + 
					(otherGameObj ? otherGameObj->getAngularVelocity() : raylib::Vector3(0)).CrossProduct(contact - otherObject->position);

	// Find the velocity of the points along the normal, ie toward eachother
	float normalVelocity1 = pointVel1.DotProduct(normal);
	float normalVelocity2 = pointVel2.DotProduct(normal);


	if (normalVelocity1 >= normalVelocity2) // They are moving closer
	{
		// The collision point from the center of mass
		raylib::Vector3 radius1 = contact - position;
		raylib::Vector3 radius2 = contact - otherObject->position;


		// Restitution (elasticity) * magnitude of delta point velocity
		float numerator = -(1 + 0.5f * (getElasticity() + (otherGameObj ? otherGameObj->getElasticity() : 1)))  *  (pointVel1 - pointVel2).Length();

		// Put simply, this is how much the collision point will resist linear velocity		N dot (((r × N) * 1/I) × r)
		float obj1Value = 1 / getMass() + normal.DotProduct(radius1.CrossProduct(normal).Transform( getMoment().Invert() ).CrossProduct(radius1));
		float obj2Value = 1 / (otherGameObj ? otherGameObj->getMass() : INFINITY) + 
			normal.DotProduct(radius2.CrossProduct(normal).Transform( (otherGameObj ? otherGameObj->getMoment().Invert() : raylib::Matrix::Identity()) ).CrossProduct(radius2));

		// Find the collision impulse
		float j = numerator / (obj1Value + obj2Value);
		raylib::Vector3 impulse = normal * j;


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
