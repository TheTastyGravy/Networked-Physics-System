#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include "../Shared/RingBuffer.h"
#include <vector>
#include <unordered_map>

#include <iostream>


#include "rlgl.h"
//function to allow a cube to be drawn with rotation
static void DrawCubeCustom(Vector3 position, Vector3 rotation, float width, float height, float length, Color color, bool thing, bool isStatic = false)
{
	rlPushMatrix();

	// NOTE: Be careful! Function order matters (rotate -> scale -> translate)
	//rlTranslatef(position.x, position.y, position.z);
	//rlScalef(2.0f, 2.0f, 2.0f);

	if (true)
	{
		raylib::Quaternion quat;
		if (thing)
		{
			quat = QuaternionFromEuler(rotation.x, rotation.y, rotation.z);
			//raylib::Matrix mat = quat.ToMatrix();
			//quat = quat.FromMatrix(mat);

			auto axisAngle = quat.ToAxisAngle();

			rlTranslatef(position.x, position.y, position.z);
			rlRotatef(axisAngle.second * RAD2DEG, axisAngle.first.x, axisAngle.first.y, axisAngle.first.z);
		}
		else
		{
			quat = QuaternionFromMatrix(MatrixRotateXYZ(rotation));

			rlMultMatrixf(MatrixToFloatV(MatrixTranspose( MatrixRotateXYZ(rotation) )).v);
			rlMultMatrixf(MatrixToFloatV(MatrixTranslate(position.x, position.y, position.z)).v);
		}

		if (isStatic)
		{
			quat = QuaternionFromEuler(rotation.x, rotation.y, rotation.z);

			auto axisAngle = quat.ToAxisAngle();
			rlRotatef(axisAngle.second * RAD2DEG, axisAngle.first.x, axisAngle.first.y, axisAngle.first.z);
		}
	}
	else
	{
		rlRotatef(rotation.x * RAD2DEG, 1, 0, 0);
		rlRotatef(rotation.y * RAD2DEG, 0, 1, 0);
		rlRotatef(rotation.z * RAD2DEG, 0, 0, 1);
	}


	DrawCube({ 0,0,0 }, width, height, length, color);
	rlPopMatrix();
}


/// <summary>
/// Used by a player to connect to a server and play the game. 
/// Handles functionality such as getting input, drawing, and client side logic (i.e. Things that dont need to be syncronised)
/// </summary>
class Client
{
public:
	Client();
	virtual ~Client();


	// ----------   TEMPARARY TEST FUNCTIONS   ----------

	void attemptToConnectToServer(const char* ip, short port)
	{
		if (peerInterface == nullptr)
		{
			peerInterface = RakNet::RakPeerInterface::GetInstance();
		}

		// Close any existing connections
		peerInterface->Shutdown(300);


		// No data is needed since we are connecting
		RakNet::SocketDescriptor sd;
		// 1 max connection since connecting
		peerInterface->Startup(1, &sd, 1);
		// Automatic pinging for timestamping
		peerInterface->SetOccasionalPing(true);

		//peerInterface->ApplyNetworkSimulator(0.1f, 50, 50);

		std::cout << "Connecting to server at: " << ip << std::endl;

		// Attempt to connect to the server
		RakNet::ConnectionAttemptResult res = peerInterface->Connect(ip, port, nullptr, 0);
		if (res != RakNet::CONNECTION_ATTEMPT_STARTED)
		{
			std::cout << "Unable to start connection, error number: " << res << std::endl;
		}
	}

	void draw()
	{
		for (auto& it : staticObjects)
		{
			DrawCubeCustom(it->getPosition(), it->getRotation(), 200, 4, 200, GREEN, false, true);
		}
		for (auto& it : gameObjects)
		{
			DrawCubeCustom(it.second->getPosition(), it.second->getRotation(), 8, 8, 8, RED, false);
			//DrawCubeCustom(it.second->getPosition(), it.second->getRotation(), 8, 8, 8, ORANGE, true);
		}
		if (myClientObject)
		{
			DrawCubeCustom(myClientObject->getPosition(), myClientObject->getRotation(), 8, 8, 8, BLACK, false);
			//DrawCubeCustom(myClientObject->getPosition(), myClientObject->getRotation(), 8, 8, 8, GRAY, true);
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
	// THESE FUNCTIONS ARE FOR THE USER TO USE WITHIN THE CLIENT CLASS

	// Perfrom an update on the system, including prediction and input
	void systemUpdate();
	// Process packets that are used by the system
	void processSystemMessage(const RakNet::Packet* packet);


	// Used by systemUpdate for prediction and to send it to the server
	virtual Input getInput()
	{
		Input input;
		input.bool1 = IsKeyDown(KEY_W);
		input.bool2 = IsKeyDown(KEY_A);
		input.bool3 = IsKeyDown(KEY_S);
		input.bool4 = IsKeyDown(KEY_D);

		return input;
	}

	// Holds information used to create objects. Static objects will only need part or it
	struct ObjectInfo
	{
		PhysicsState state;
		// The objects collider, or null pointer if it doesnt have one
		Collider* collider = nullptr;
		float mass = 1, elasticity = 1;
	};
	
	/// <summary>
	/// Factory method used to create custom static objects
	/// </summary>
	/// <param name="objectInfo">System defined information for the object. Not all information is nessesary fro static objects</param>
	/// <param name="bsIn">Custom paramiters. Only read as much is nessesary for the object</param>
	/// <returns>A pointer to the new static object</returns>
	virtual StaticObject* staticObjectFactory(unsigned int typeID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn)
	{
		return new StaticObject(objectInfo.state.position, objectInfo.state.rotation, objectInfo.collider);
	}
	/// <summary>
	/// Factory method used to create custom game objects
	/// </summary>
	/// <param name="objectID">The object ID that needs to be given to the object. If the objects ID does not match, it will be deleted</param>
	/// <param name="objectInfo">System defined information for the object</param>
	/// <param name="bsIn">Custom paramiters</param>
	/// <returns>A pointer to the new game object</returns>
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn)
	{
		return new GameObject(objectInfo.state, objectID, objectInfo.mass, objectInfo.elasticity, objectInfo.collider);
	}
	/// <summary>
	/// Factory method used to create a custom client object. Use clientID for the objects ID
	/// </summary>
	/// <param name="objectInfo">System defined information for the object</param>
	/// <param name="bsIn">Custom paramiters</param>
	/// <returns>A pointer to the new client object</returns>
	virtual ClientObject* clientObjectFactory(unsigned int typeID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn)
	{
		return new ClientObject(objectInfo.state, getClientID(), objectInfo.mass, objectInfo.elasticity, objectInfo.collider);
	}


	unsigned int getClientID() const { return clientID; }

private:
	// THESE FUNCTIONS ARE ONLY USED INTERNALLY BY THE SYSTEM, AND ARE NOT FOR THE USER

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



protected:
	RakNet::RakPeerInterface* peerInterface;

	// Static objects are receved from the server after connecting
	std::vector<StaticObject*> staticObjects;
	// All game objects. This includes other players client objects
	// <object ID, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	// The object owned by this client
	ClientObject* myClientObject;

private:
	// The ID assigned to this client by the server
	// The user should not be able to change this, so give them a getter
	unsigned int clientID;

	// Used to determine delta time
	RakNet::Time lastUpdateTime;

	// Buffer of <time of input, state at time, player input>
	RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input>> inputBuffer;
};