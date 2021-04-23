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


void GameObject::applyForce(raylib::Vector3 force, raylib::Vector3 position)
{
	velocity += force / getMass();
	//figure out how torque works
	//angularVelocity += (force.y * position.x + force.x * position.y) / getMoment();
}


//	----------   THIS FUNCTION IS ALMOST CERTAINLY BROKEN   ----------
void GameObject::resolveCollision(StaticObject* otherObject, raylib::Vector3 contact, raylib::Vector3 collisionNormal, float pen)
{
	// Find the vector between their centers, or use the provided
	// direction of force, and make sure its normalized
	raylib::Vector3 normal = Vector3Normalize(collisionNormal == raylib::Vector3(0) ? otherObject->position - position : collisionNormal);

	//does not work in 3d: use cross. this is also bad because perp in 3d is infinite
	// Get the vector perpendicular to the collision normal
	raylib::Vector3 perpColNorm(normal.y, -normal.x);


	// If the other object is not static, cast it to a game object
	GameObject* otherGameObj = otherObject->isStatic() ? nullptr : otherGameObj = static_cast<GameObject*>(otherObject);


	//this is good for 2d, prob not 3d

	// These are applied to the radius from axis to the application of force
	// The radius from the center of mass to the contact point
	float radius1 = Vector3DotProduct(contact - position, -perpColNorm);
	float radius2 = Vector3DotProduct(contact - otherObject->position, perpColNorm);


	//proper calculation for point velocity
	//linear + cross(angular, (contact - position))

	//this is likely a problem area

	// Velocity of the contact point on this object
	raylib::Vector3 cp_velocity1 = velocity - angularVelocity * radius1;
	// Velocities of contact point of the other object
	raylib::Vector3 cp_velocity2 = (otherGameObj ? otherGameObj->getVelocity() : raylib::Vector3(0)) +
								   (otherGameObj ? otherGameObj->getAngularVelocity() : raylib::Vector3(0)) * radius2;

	float cp_vel01 = cp_velocity1.DotProduct(normal);
	float cp_vel02 = cp_velocity2.DotProduct(normal);



	if (cp_vel01 > cp_vel02) // They are moving closer
	{
		// This will calculate the effective mass at the point of each 
		// object (How much it will move due to the forces applied)
		float mass1 = 1.0f / (1.0f / mass + (radius1 * radius1) / getMoment());
		float mass2 = 1.0f / (1.0f / (otherGameObj ? otherGameObj->getMass() : INFINITY) + (radius2 * radius2) / (otherGameObj ? otherGameObj->getMoment() : 0.0001f));

		// Use the average elasticity of the colliding objects
		float elasticity = 0.5f * (getElasticity() + (otherGameObj ? otherGameObj->getElasticity() : 1));

		raylib::Vector3 impact = normal * ((1.0f + elasticity) * mass1 * mass2 / (mass1 + mass2) * (cp_vel01 - cp_vel02));
		applyForce(-impact, contact - position);
		if (otherGameObj)
		{
			otherGameObj->applyForce(impact, contact - otherObject->position);
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


void GameObject::updateState(const PhysicsState& state, RakNet::TimeMS stateTime, RakNet::TimeMS currentTime)
{
	// If we are more up to date than this packet, ignore it
	if (stateTime < lastPacketTime)
	{
		return;
	}

	float deltaTime = (currentTime - stateTime) * 0.001f;

	PhysicsState newState(state);
	// Extrapolate to get the state at the current time
	newState.position += newState.velocity * deltaTime;
	newState.rotation += newState.angularVelocity * deltaTime;

	// Update our state
	position = newState.position;
	rotation = newState.rotation;
	velocity = newState.velocity;
	angularVelocity = newState.angularVelocity;


	lastPacketTime = stateTime;
}

void GameObject::applyStateDif(const PhysicsState& stateDif, RakNet::TimeMS stateTime, RakNet::TimeMS currentTime)
{

}
