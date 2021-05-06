#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include <vector>
#include <unordered_map>
#include "../Shared/RingBuffer.h"


#include "rlgl.h"
//function to allow a cube to be drawn with rotation
static void DrawCubeCustom(Vector3 position, Vector3 rotation, float width, float height, float length, Color color)
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	rlPushMatrix();

	// NOTE: Be careful! Function order matters (rotate -> scale -> translate)
	rlTranslatef(position.x, position.y, position.z);
	//rlScalef(2.0f, 2.0f, 2.0f);
	rlRotatef(rotation.x * RAD2DEG, 1, 0, 0);
	rlRotatef(rotation.y * RAD2DEG, 0, 1, 0);
	rlRotatef(rotation.z * RAD2DEG, 0, 0, 1);


	rlBegin(RL_TRIANGLES);
	rlColor4ub(color.r, color.g, color.b, color.a);

	// Front Face -----------------------------------------------------
	rlVertex3f(x - width / 2, y - height / 2, z + length / 2);  // Bottom Left
	rlVertex3f(x + width / 2, y - height / 2, z + length / 2);  // Bottom Right
	rlVertex3f(x - width / 2, y + height / 2, z + length / 2);  // Top Left

	rlVertex3f(x + width / 2, y + height / 2, z + length / 2);  // Top Right
	rlVertex3f(x - width / 2, y + height / 2, z + length / 2);  // Top Left
	rlVertex3f(x + width / 2, y - height / 2, z + length / 2);  // Bottom Right

	// Back Face ------------------------------------------------------
	rlVertex3f(x - width / 2, y - height / 2, z - length / 2);  // Bottom Left
	rlVertex3f(x - width / 2, y + height / 2, z - length / 2);  // Top Left
	rlVertex3f(x + width / 2, y - height / 2, z - length / 2);  // Bottom Right

	rlVertex3f(x + width / 2, y + height / 2, z - length / 2);  // Top Right
	rlVertex3f(x + width / 2, y - height / 2, z - length / 2);  // Bottom Right
	rlVertex3f(x - width / 2, y + height / 2, z - length / 2);  // Top Left

	// Top Face -------------------------------------------------------
	rlVertex3f(x - width / 2, y + height / 2, z - length / 2);  // Top Left
	rlVertex3f(x - width / 2, y + height / 2, z + length / 2);  // Bottom Left
	rlVertex3f(x + width / 2, y + height / 2, z + length / 2);  // Bottom Right

	rlVertex3f(x + width / 2, y + height / 2, z - length / 2);  // Top Right
	rlVertex3f(x - width / 2, y + height / 2, z - length / 2);  // Top Left
	rlVertex3f(x + width / 2, y + height / 2, z + length / 2);  // Bottom Right

	// Bottom Face ----------------------------------------------------
	rlVertex3f(x - width / 2, y - height / 2, z - length / 2);  // Top Left
	rlVertex3f(x + width / 2, y - height / 2, z + length / 2);  // Bottom Right
	rlVertex3f(x - width / 2, y - height / 2, z + length / 2);  // Bottom Left

	rlVertex3f(x + width / 2, y - height / 2, z - length / 2);  // Top Right
	rlVertex3f(x + width / 2, y - height / 2, z + length / 2);  // Bottom Right
	rlVertex3f(x - width / 2, y - height / 2, z - length / 2);  // Top Left

	// Right face -----------------------------------------------------
	rlVertex3f(x + width / 2, y - height / 2, z - length / 2);  // Bottom Right
	rlVertex3f(x + width / 2, y + height / 2, z - length / 2);  // Top Right
	rlVertex3f(x + width / 2, y + height / 2, z + length / 2);  // Top Left

	rlVertex3f(x + width / 2, y - height / 2, z + length / 2);  // Bottom Left
	rlVertex3f(x + width / 2, y - height / 2, z - length / 2);  // Bottom Right
	rlVertex3f(x + width / 2, y + height / 2, z + length / 2);  // Top Left

	// Left Face ------------------------------------------------------
	rlVertex3f(x - width / 2, y - height / 2, z - length / 2);  // Bottom Right
	rlVertex3f(x - width / 2, y + height / 2, z + length / 2);  // Top Left
	rlVertex3f(x - width / 2, y + height / 2, z - length / 2);  // Top Right

	rlVertex3f(x - width / 2, y - height / 2, z + length / 2);  // Bottom Left
	rlVertex3f(x - width / 2, y + height / 2, z + length / 2);  // Top Left
	rlVertex3f(x - width / 2, y - height / 2, z - length / 2);  // Bottom Right
	rlEnd();
	rlPopMatrix();
}


class Client
{
public:
	Client();
	virtual ~Client();


	// Try to connect to a server
	// Not part of the system, so remove later
	void attemptToConnectToServer(const char* ip, short port);


	//temp
	void draw()
	{
		for (auto& it : staticObjects)
		{
			DrawCubeCustom(it->position, it->rotation, 200, 4, 200, GREEN);
		}
		for (auto& it : gameObjects)
		{
			DrawCubeCustom(it.second->position, it.second->rotation, 8, 8, 8, RED);
			//DrawSphere(it.second->position, 4, RED);
		}
		if (myClientObject)
		{
			DrawCubeCustom(myClientObject->position, myClientObject->rotation, 8, 8, 8, BLACK);
			//DrawSphere(myClientObject->position, 4, BLACK);
		}
	}

	void loop()
	{
		RakNet::Packet* packet = nullptr;

		for (packet = peerInterface->Receive(); packet; peerInterface->DeallocatePacket(packet),
			packet = peerInterface->Receive())
		{
			processSystemMessage(packet);
		}

		systemUpdate();
	}


protected:
	// Perform a physics step on objects with prediction. calls getInput
	void systemUpdate();

	// Processes the packet if it is used by the system
	void processSystemMessage(const RakNet::Packet* packet);


	// Abstract
	// User defined function for getting player input to send to the server
	// Input struct is defined in ClientObject.h
	virtual Input getInput()
	{
		Input input;
		input.b1 = IsKeyDown(KEY_W);
		input.b2 = IsKeyDown(KEY_A);
		input.b3 = IsKeyDown(KEY_S);
		input.b4 = IsKeyDown(KEY_D);

		return input;
	}


	// Holds information used to create objects. Static objects do not use all of it
	struct ObjectInfo
	{
		PhysicsState state;
		Collider* collider = nullptr;
		float mass, elasticity;
	};
	
	// Abstract
	// User defined factory method to create static objects when connecting to a server
	virtual StaticObject* staticObjectFactory(unsigned int typeID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn)
	{
		return new StaticObject(objectInfo.state.position, objectInfo.state.rotation, objectInfo.collider);
	}
	// Abstract
	// User defined factory method to create game objects (including other client objects)
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn)
	{
		return new GameObject(objectInfo.state, objectID, objectInfo.mass, objectInfo.elasticity, objectInfo.collider);
	}
	// Abstract
	// User defined factory method to create the client object for this Client instance
	virtual ClientObject* clientObjectFactory(unsigned int typeID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn)
	{
		return new ClientObject(objectInfo.state, getClientID(), objectInfo.mass, objectInfo.elasticity, objectInfo.collider);
	}


private:
	// Read a collider from a bit stream. Instantiated with new
	Collider* readCollider(RakNet::BitStream& bsIn);

	// Create static object instances from data
	void createStaticObjects(RakNet::BitStream& bsIn);
	// Create a game object instance from data
	void createGameObject(RakNet::BitStream& bsIn);
	// Create the client object we own from data
	void createClientObject(RakNet::BitStream& bsIn);

	// Destroy the game object of objectID
	void destroyGameObject(unsigned int objectID);
	// Destroy all staticObjects, gameObjects, and myClientObject
	void destroyAllObjects();

	// Used when an object update is receved from the server
	void applyServerUpdate(RakNet::BitStream& bsIn, const RakNet::Time& timeStamp);


private:
	// The ID assigned to this client by the server
	// The user should not be able to change this, so give them a getter
	unsigned int clientID;
protected:
	unsigned int getClientID() const { return clientID; }


protected:
	RakNet::RakPeerInterface* peerInterface;

	// Used to determine delta time
	RakNet::Time lastUpdateTime;

	// Objects used for static geometry
	std::vector<StaticObject*> staticObjects;
	//<object ID, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	// The object owned by this client
	ClientObject* myClientObject;


	// Buffer of <time of input, state at time, player input>
	RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input>> inputBuffer;
};