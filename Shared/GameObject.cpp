#include "GameObject.h"

#include <iostream>


GameObject::GameObject() :
	StaticObject(), objectID(-1), lastPacketTime(0), 
	velocity(0, 0, 0), angularVelocity(0, 0, 0), mass(1), elasticity(1), linearDrag(0), angularDrag(0)
{
	// Game objects are not static
	bIsStatic = false;
	typeID = -1;

	// Use the collider to get moment. If mass or the collider is ever changed, this should be recalculated
	moment = (getCollider() ? getCollider()->calculateInertiaTensor(mass) : MatrixIdentity());
}

GameObject::GameObject(raylib::Vector3 position, raylib::Vector3 rotation, unsigned int objectID, float mass, float elasticity, Collider* collider, float linearDrag, float angularDrag) :
	StaticObject(position, rotation, collider), objectID(objectID), lastPacketTime(0),
	velocity(0,0,0), angularVelocity(0,0,0), mass(mass), elasticity(elasticity), linearDrag(linearDrag), angularDrag(angularDrag)
{
	// Game objects are not static
	bIsStatic = false;
	typeID = -1;

	// Use the collider to get moment. If mass or the collider is ever changed, this should be recalculated
	moment = (getCollider() ? getCollider()->calculateInertiaTensor(mass) : MatrixIdentity());
}

GameObject::GameObject(PhysicsState initState, unsigned int objectID, float mass, float elasticity, Collider* collider, float linearDrag, float angularDrag) :
	StaticObject(initState.position, initState.rotation, collider), objectID(objectID), lastPacketTime(0), 
	velocity(initState.velocity), angularVelocity(initState.angularVelocity), 
	mass(mass), elasticity(elasticity), linearDrag(linearDrag), angularDrag(angularDrag)
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
	
	// [typeID, collider info, position, rotation]
	StaticObject::serialize(bs);

	// Moment is generated from collider, so doesnt need to be sent
	bs.Write(velocity);
	bs.Write(angularVelocity);
	bs.Write(mass);
	bs.Write(elasticity);
}


void GameObject::physicsStep(float timeStep)
{
	// Apply velocity
	position += velocity * timeStep;
	rotation += angularVelocity * timeStep;

	// Apply drag
	velocity -= velocity * linearDrag * timeStep;
	angularVelocity -= angularVelocity * angularDrag * timeStep;


	auto cmp = [](float A, float B)
	{
		float diff = A - B;
		return (diff < 0.01f && diff > -0.01f);
	};

	if (cmp(velocity.x, 0))
		velocity.x = 0;
	if (cmp(velocity.y, 0))
		velocity.y = 0;
	if (cmp(velocity.z, 0))
		velocity.z = 0;
	
	if (cmp(angularVelocity.x, 0))
		angularVelocity.x = 0;
	if (cmp(angularVelocity.y, 0))
		angularVelocity.y = 0;
	if (cmp(angularVelocity.z, 0))
		angularVelocity.z = 0;


	//temp gravity
	applyForce(raylib::Vector3(0, -1, 0) * mass, { 0,0,0 }, false);
}


void GameObject::applyForce(const raylib::Vector3& force, const raylib::Vector3& relitivePosition, bool temp)
{
	velocity += Vector3Scale(force, 1 / getMass());


	//rotate torque by rotation in get to local space - broken
	raylib::Vector3 vec = Vector3Transform(Vector3Transform( Vector3CrossProduct(relitivePosition, force), getMoment().Invert()), MatrixRotateXYZ(rotation) );
	angularVelocity -= vec;

	if (temp)
	{
		std::cout << "angular change: " << vec.x << " " << vec.y << " " << vec.z << std::endl;
	}
}

void GameObject::resolveCollision(StaticObject* otherObject, const raylib::Vector3& contact, const raylib::Vector3& collisionNormal, bool isOnServer, bool shouldAffectOther)
{
	if (otherObject == nullptr)
	{
		return;
	}

	raylib::Vector3 normal = Vector3Normalize(collisionNormal);

	// If the other object is not static, cast it to a game object
	GameObject* otherGameObj = otherObject->isStatic() ? nullptr : static_cast<GameObject*>(otherObject);


	// The collision point from the center of mass
	raylib::Vector3 radius1 = Vector3Subtract(contact, position);
	raylib::Vector3 radius2 = Vector3Subtract(contact, otherObject->position);
	std::cout << "radius: " << radius1.x << " " << radius1.y << " " << radius1.z << std::endl;

	// Find the velocities of the contact points
	raylib::Vector3 pointVel1 = velocity + angularVelocity.CrossProduct(radius1);
	raylib::Vector3 pointVel2 = (otherGameObj ? otherGameObj->getVelocity() : raylib::Vector3(0)) + 
					(otherGameObj ? otherGameObj->getAngularVelocity() : raylib::Vector3(0)).CrossProduct(radius2);

	raylib::Vector3 relitiveVelocity = pointVel1 - pointVel2;
	std::cout << "rel vel: " << relitiveVelocity.x << " " << relitiveVelocity.y << " " << relitiveVelocity.z << std::endl;

	if (relitiveVelocity.DotProduct(normal) > 0) // They are moving closer
	{
		// Combined inverse mass of the objects
		float inverseMass = 1 / getMass() + (otherGameObj ? 1 / otherGameObj->getMass() : 0);
		raylib::Matrix invMoment1 = getMoment().Invert();
		raylib::Matrix invMoment2 = (otherGameObj ? otherGameObj->getMoment().Invert() : raylib::Matrix());


		//	----------   Normal Impulse   ----------

		// Restitution (elasticity) * magnitude of delta point velocity
		float numerator = -(1 + 0.5f * (getElasticity() + (otherGameObj ? otherGameObj->getElasticity() : 0))) * relitiveVelocity.DotProduct(normal);
		std::cout << "numerator: " << numerator << std::endl;
		// Put simply, this is how much the collision point will resist linear velocity
		float inverseMassSumNorm = inverseMass;
		inverseMassSumNorm += normal.DotProduct(radius1.CrossProduct(normal).Transform(invMoment1).CrossProduct(radius1));
		inverseMassSumNorm += normal.DotProduct(radius2.CrossProduct(normal).Transform(invMoment2).CrossProduct(radius2));
		std::cout << "mass sum: " << inverseMassSumNorm << std::endl;

		// Find and apply normal impulse
		float j = (numerator / inverseMassSumNorm);
		raylib::Vector3 normalImpulse = normal * j;


		auto cmp = [](float A, float B)
		{
			float diff = A - B;
			return (diff < 0.01f && diff > -0.01f);
		};

		if (cmp(normalImpulse.x, 0))
			normalImpulse.x = 0;
		if (cmp(normalImpulse.y, 0))
			normalImpulse.y = 0;
		if (cmp(normalImpulse.z, 0))
			normalImpulse.z = 0;


		applyForce(normalImpulse, radius1, true);
		if (otherGameObj && shouldAffectOther)
		{
			otherGameObj->applyForce(-normalImpulse, radius2, true);
		}

		std::cout << "N Force: " << normalImpulse.x << " " << normalImpulse.y << " " << normalImpulse.z << std::endl << std::endl;


		//	----------   Friction Impulse   ----------

		raylib::Vector3 tangent = Vector3Normalize(relitiveVelocity - normal * Vector3DotProduct(relitiveVelocity, normal));
		
		float inverseMassSumTan = inverseMass;
		inverseMassSumTan += tangent.DotProduct(radius1.CrossProduct(tangent).Transform(invMoment1).CrossProduct(radius1));
		inverseMassSumTan += tangent.DotProduct(radius2.CrossProduct(tangent).Transform(invMoment2).CrossProduct(radius2));
		
		// Find and apply friction impulse
		float jF = Vector3DotProduct(-relitiveVelocity, tangent) / inverseMassSumTan;
		raylib::Vector3 frictionImpulse = tangent * jF;
		
		
		if (cmp(frictionImpulse.x, 0))
			frictionImpulse.x = 0;
		if (cmp(frictionImpulse.y, 0))
			frictionImpulse.y = 0;
		if (cmp(frictionImpulse.z, 0))
			frictionImpulse.z = 0;
		
		
		//applyForce(-frictionImpulse, radius1);
		velocity += Vector3Scale(frictionImpulse, 1 / getMass());
		angularVelocity += Vector3Transform(Vector3CrossProduct(radius1, frictionImpulse), getMoment().Invert());
		if (otherGameObj && shouldAffectOther)
		{
			//otherGameObj->applyForce(frictionImpulse, radius2);
			otherGameObj->velocity += Vector3Scale(-frictionImpulse, 1 / otherGameObj->getMass());
			otherGameObj->angularVelocity += Vector3Transform(Vector3CrossProduct(radius2, -frictionImpulse), otherGameObj->getMoment().Invert());
		}
		
		std::cout << "F Force: " << frictionImpulse.x << " " << frictionImpulse.y << " " << frictionImpulse.z << std::endl << std::endl;



		// Trigger collision events
		if (isOnServer)
		{
			server_onCollision(otherObject);
			if (otherGameObj && shouldAffectOther)
			{
				otherGameObj->server_onCollision(this);
			}
		}
		else
		{
			client_onCollision(otherObject);
			if (otherGameObj && shouldAffectOther)
			{
				otherGameObj->client_onCollision(this);
			}
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
		else if (dist > smooth_threshold) // If we are only a small distance from the real value, we dont move
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
		else if (dist > smooth_threshold) // If we are only a small distance from the new value, we dont move
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
