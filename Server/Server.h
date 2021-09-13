#pragma once
#include "raylib-cpp.hpp"
#include <RakPeerInterface.h>
#include <vector>
#include <deque>
#include <unordered_map>
#include <GetTime.h>
#include "../Shared/ClientObject.h"
#include "../Shared/Sphere.h"
#include "../Shared/OBB.h"


/// <summary>
/// Used to authoritativly controll the game. Anything that needs to be syncronised across clients, should be done here
/// </summary>
class Server
{
public:
	Server(float timeStep = 0.01f, float playoutDelay = 0.05f);
	virtual ~Server();


	// THESE FUNCTIONS CAN BE USED WITHIN AND OUTSIDE OF THE SERVER CLASS, I.E. BY GAME OBJECTS

	/// <summary>
	/// Create a new game object. Creation will be syncronised across clients
	/// </summary>
	/// <param name="typeID">The ID for the object type to be created. Custom object classes need to have unqiue IDs</param>
	/// <param name="state">The physics state the game object should be created with</param>
	/// <param name="creationTime">A time in the past, for when an object is created in responce to an input</param>
	/// <param name="customParamiters">Additional paramiters used for custom classes</param>
	void createObject(unsigned int typeID, const PhysicsState& state, const RakNet::Time& creationTime, RakNet::BitStream* customParamiters = nullptr);
	// Destroy the object with the passed ID. Destruction will be syncronised across clients
	void destroyObject(unsigned int objectID);
	
protected:
	// THESE FUNCTIONS ARE FOR THE USER TO USE WITHIN THE SERVER CLASS

	// Perfrom an update on the system, including physics and syncronisation
	void systemUpdate();
	// Process packets that are used by the system
	void processSystemMessage(const RakNet::Packet* packet);


	/// <summary>
	/// Factory method used to create custom game objects
	/// </summary>
	/// <param name="objectID">The ID that needs to be assigned to the new object. If the objects ID does not match, it will be deleted</param>
	/// <param name="state">The state that should be given to the object</param>
	/// <param name="bsIn">Custom paramiters to the specified type</param>
	/// <returns>A pointer to the new game object</returns>
	virtual GameObject* gameObjectFactory(unsigned int typeID, unsigned int objectID, const PhysicsState& state, RakNet::BitStream& bsIn) = 0;
	/// <summary>
	/// Factory method used to create custom client objects
	/// </summary>
	/// <param name="clientID">The ID that needs to be assigned to the new object. If the objects ID does not match, it will be deleted</param>
	/// <returns>A pointer to the new client object</returns>
	virtual ClientObject* clientObjectFactory(unsigned int clientID) = 0;


	RakNet::Time getTime() const { return lastUpdateTime; }

private:
	// THESE FUNCTIONS ARE ONLY USED INTERNALLY BY THE SYSTEM, AND ARE NOT FOR THE USER

	// Check for collisions and resolve them
	void collisionDetectionAndResolution();

	// Used to send data to a new client including client ID, static objects, game objects, and thier client object
	void onClientConnect(const RakNet::SystemAddress& connectedAddress);
	// Used when a client disconnects. Sends messgae to all other clients to destroy their client object, and removes it from the server
	void onClientDisconnect(const RakNet::SystemAddress& disconnectedAddress);

	// Process player input
	void processInput(const RakNet::SystemAddress& address, RakNet::BitStream& bsIn);

	// Update client objects, using buffered input
	void updateClientObject(unsigned int clientID, const RakNet::Time& time);

	// Broadcast a message containing the game objects physics state. (Does not use serialize)
	void sendGameObjectUpdate(GameObject* object, RakNet::Time timeStamp);



protected:
	RakNet::RakPeerInterface* peerInterface;
	
	// Objects used for static geometry. Need to be created on startup
	std::vector<StaticObject*> staticObjects;
	// All game objects that currently exist. Use createObject and destroyObject, do not insert or erase elements
	// <object id, game object>
	std::unordered_map<unsigned int, GameObject*> gameObjects;
	// All client objects that currently exist. The system will create and delete them
	// <client id, client object>
	std::unordered_map<unsigned int, ClientObject*> clientObjects;

	// Used to determine the client ID from a packets address
	// <client address, client ID>
	std::unordered_map<unsigned long, unsigned int> addressToClientID;

private:
	// Contains the playout buffer as well as related data
	class PlayoutBuffer
	{
	public:
		PlayoutBuffer() :
			buffer(), initTime(0), timeBetweenFrames(0), currentFrame(0)
		{};

		// Used for each entry in a playout buffer
		struct InputData
		{
			InputData() :
				frame(0), input(Input()), actionHasBeenPerformed(false)
			{}
			InputData(unsigned int frame, Input input, bool actionHasBeenPerformed) :
				frame(frame), input(input), actionHasBeenPerformed(actionHasBeenPerformed)
			{}

			unsigned int frame;
			Input input;
			bool actionHasBeenPerformed;
		};
		// The playout buffer
		std::deque<InputData> buffer;

		// The time for frame 0
		RakNet::Time initTime;
		// The time between frames
		RakNet::Time timeBetweenFrames;
		// The frame currently being simulated
		unsigned int currentFrame;
	};
	// Delayed playout buffer for client inputs
	// <client ID, playout buffer>
	std::unordered_map<unsigned int, PlayoutBuffer> playoutBuffers;
	
	// Object IDs to be destroied at the end of this update
	std::vector<unsigned int> deadObjects;

	// Time in milliseconds. Multiply by 0.001 for seconds
	RakNet::Time lastUpdateTime;
	// The time between update cycles
	const RakNet::Time timeStep;
	// The delay used for processing player input
	const RakNet::Time playoutDelay;

	// Identifier for a client and the ClientObject they own. ALWAYS INCREMENT AFTER USE
	unsigned int nextClientID = 1;
	// Identifier for a GameObject. ALWAYS INCREMENT AFTER USE
	unsigned int nextObjectID = 101;
};