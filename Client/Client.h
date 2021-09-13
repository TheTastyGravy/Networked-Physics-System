#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include "../Shared/ClientObject.h"
#include "../Shared/RingBuffer.h"
#include <vector>
#include <unordered_map>


/// <summary>
/// Used by a player to connect to a server and play the game. 
/// Handles functionality such as getting input, drawing, and client side logic (i.e. Things that dont need to be syncronised)
/// </summary>
class Client
{
public:
	Client(float timeBetweenInputMessages = 0.03f, int maxInputsPerMessage = 5);
	virtual ~Client();

protected:
	// THESE FUNCTIONS ARE FOR THE USER TO USE WITHIN THE CLIENT CLASS

	// Perfrom an update on the system, including prediction and input
	void systemUpdate();
	// Process packets that are used by the system
	void processSystemMessage(const RakNet::Packet* packet);


	// Used by systemUpdate for prediction and to send it to the server
	virtual Input getInput() = 0;

	// Holds information used to create objects. Static objects will only need part or it
	struct ObjectInfo
	{
		PhysicsState state;
		// The objects collider, or null pointer if it doesnt have one
		Collider* collider = nullptr;
		float mass = 1, elasticity = 1, friction = 1;
	};
	
	/// <summary>
	/// Factory method used to create custom static objects
	/// </summary>
	/// <param name="objectInfo">System defined information for the object. Not all information is nessesary fro static objects</param>
	/// <param name="bsIn">Custom paramiters. Only read as much is nessesary for the object</param>
	/// <returns>A pointer to the new static object</returns>
	virtual StaticObject* staticObjectFactory(unsigned int typeID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn) = 0;
	/// <summary>
	/// Factory method used to create custom game objects
	/// </summary>
	/// <param name="objectID">The object ID that needs to be given to the object. If the objects ID does not match, it will be deleted</param>
	/// <param name="objectInfo">System defined information for the object</param>
	/// <param name="bsIn">Custom paramiters</param>
	/// <returns>A pointer to the new game object</returns>
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn) = 0;
	/// <summary>
	/// Factory method used to create a custom client object. Use clientID for the objects ID
	/// </summary>
	/// <param name="objectInfo">System defined information for the object</param>
	/// <param name="bsIn">Custom paramiters</param>
	/// <returns>A pointer to the new client object</returns>
	virtual ClientObject* clientObjectFactory(unsigned int typeID, ObjectInfo& objectInfo, RakNet::BitStream& bsIn) = 0;


	unsigned int getClientID() const { return clientID; }
	RakNet::Time getTime() const { return lastUpdateTime; }

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
	// Called when the server acks a message. Used for sending input
	void checkAckReceipt(RakNet::BitStream& bsIn);
	// Send player input to the server. Should be called every couple frames
	void sendInput();



protected:
	RakNet::RakPeerInterface* peerInterface;

	// Static objects are receved from the server after connecting
	std::vector<StaticObject*> staticObjects;
	// All game objects. This includes other players client objects
	// <object ID, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	// The object owned by this client
	ClientObject* myClientObject;

	// The time between sending player input to the server. All unsent inputs will be sent at once
	const float timeBetweenInputMessages;
	// The maximum number of inputs that can be sent. If there are more unsent inputs, the server will likely never recieve them
	const int maxInputsPerMessage;

private:
	// The ID assigned to this client by the server
	// The user should not be able to change this, so give them a getter
	unsigned int clientID;

	// The servers delay on processing inputs. Sent with our client object
	RakNet::Time serverPlayoutDelay;
	// Used to determine delta time
	RakNet::Time lastUpdateTime;
	// The time we last sent input to the server
	RakNet::Time lastInputSent;

	// Buffer of <time of input, state at time, player input, receipt number>
	RingBuffer<std::tuple<RakNet::Time, PhysicsState, Input, uint32_t>> inputBuffer;
	// The last receipt number recieved for input
	uint32_t lastInputReceipt;

	// Object IDs that have been destroied, but not created. Caused by latency variance
	std::vector<unsigned int> objectIDBlacklist;
};