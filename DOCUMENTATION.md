# Overview
This will assume some knowlege of networking, raknet, and raylib, and their use will not be explained here. 
This system uses an authoritative server model to syncronize the state of physics objects between clients, and implements client side prediction for player movement and physics.

The system is split into 3 parts: Server, Client, and Shared, each having their own project compiling to a .lib file. Each part will be covered in detail seperatly, but the 
basic archetecture is as follows:
- A server maintains all static, physics, and client owned objects, sending updates to connected clients as they are updated, and processing player input receved from clients.
- A client connects to a server, which will send all current objects that currently exist. The client will receve updates from the server and correct any deviences. Player 
input is sent to the server, with the resultant movement predicted localy.
- Static objects are used to create a game level, having a collider but not reacting to physics, and thus are never updated.
- Game objects derive from static objects, reacting to physics and are syncronized between clients by the server.
- Client objects are effectivly game objects that are owned by a client and controlled by the player.


# Server
The server is the center of the system. It is responsible for keeping all clients in sync, processing player input, and holds the only 'correct' copy of the game state. 
As the system uses an authoritative server model, anything important should be done on the server and the result sent to clients, not clients sending results to the server.

## Objects
Static objects need to be created and pushed to `staticObjects` on startup, and never changed, as they are only sent to clients when they join.

Game objects can be created at any time using the provided functions. The system takes care of physics and collisions, and will send updates to clients every time the system 
is updated. They are acessable using `gameObjects`, where they are indexed using their object ID.

Client objects are created when a client connects, and are deleted when they disconnect. They are only updated when input is receved from a client, and are not extrapolated to 
the current time. Instead they exist in the past by half of their owners latency. They are acessable using `clientObjects`, where they are indexed using their owners clientID.

## Functions
Server has 6 important functions:

`void systemUpdate()` Updates physics for game objects, handles collisions, and sends game object updates to connected clients. This should be called every tick.

`void processSystemMessage(const Packet* packet)` Processes the packet if it is used by the system. All packets should be passed through this function.

`GameObject* gameObjectFactory(uint typeID, uint objectID, const PhysicsState& state, BitStream& bsIn)` Called by the system when a game object needs to be created. This is an 
abstract factory method that needs to be defined. This function exists to allow custom objects to be created. `typeID` coresponds to the type ID of the class that needs to be 
created, with `bsIn` providing addition data needed to create it, in addition to its `objectID` and `state`. The new object should be instantiated on the heap, with a pointer 
to it being returned.

`ClientObject* clientObjectFactory(uint clientID)` Called by the system when a client connects and a new client object needs to be created for them. This is an abstract factory 
method that needs to be defined. This is similar to `gameObjectFactory(...)` except it is used to create client objects rather than game objects, and as a consequence there are 
no paramiters since client objects are created when a client connects to a server.

`void createObject(uint typeID, const PhysicsState& state, const Time& creationTime, BitStream* customParamiters)` Public function used to create game objects, being 
syncronized across clients. `typeID`, `state`, and `customParamiters` are passed on to `gameObjectFactory(...)`, with `creationTime` being avalable for prediction: if an object 
is being created in responce to an input that occured in the past, then `creationTime` should be the time stamp of the input, and dead reckoning will be used to predict the 
current position of the object from `state`. If no prediction is needed, the current time should be used instead.

`void destroyObject(uint objectID)` Public function used to destroy game objects, being syncronized across clients. This may be called by the server, or by objects (e.g. a game 
object destroying itself after hitting something).

## Usage
A custom class needs to inherit from the server class, implementing `gameObjectFactory(...)` and `clientObjectFactory(...)`, calling `systemUpdate()` regularly and passing 
packets to `processSystemMessage(...)`. On startup, `peerInterface` needs to be setup with `SetOccasionalPing(true)`, and any static objects need to be created.

All important game logic should be done in a game loop that updates the system. While no event functions are provided for things like clients connecting or disconnecting, the 
packets that are used to determine the fact can be used to the same effect. Custom messages can be broadcast to clients to provide updates for anything that does not relate to 
physics, such as the state of game objects or damage delt to a player.

To determine the ID of a client that you have receved a message from, `addressToClientID` can be used. When a client connects, its address is mapped to its client ID, so clients 
dont have to pass their ID with every message.



# Client
Clients are used by players to play the game. They need to be connected to a server to receve existing objects and subsequent updates for them. Between updates for game objects, 
dead reckoning is used to predict their state, but server updates will override predictions if they differ. Client side prediction is used to predict the movement of the client 
object owned by this instance, making inputs feel more responsive.

While the server is authoritative and important logic should be done on it and not in clients, less important things such as effects can be done on clients, as it has no impact 
on game state.

## Objects
Static objects are receved when connecting to a server and are deleted on disconnecting, stored in `staticObjects`. They should not be changed, as the server never sends updates 
for them.

Game objects are created and deleted in responce to server messages, and are stored in `gameObjects` and are indexed by the objects ID. This also includes client objects that 
belong to other clients connected to the server, but as we dont have player input for other clients, they are treated as game objects.

The only client object stored is the one owned by this instance, in `myClientObject`. It is unique from other objects in that it is not the same as the version the server keeps 
due to client side prediction of player input. It is predicted ahead of the server by our latency.

## Functions
Client has 6 important functions:

`void systemUpdate()` Performs physics prediction on game objects, calls `getInput()` to send input to the server and update prediction for the client object. This should be 
called every frame.

`void processSystemMessage(const Packet* packet)` Processes the packet if it is used by the system. All packets should be passed through this function.

`Input getInput()` Called by the system durring an update to get player input. This is an abstract factory method that needs to be defined.

`StaticObject* staticObjectFactory(uint typeID, ObjectInfo& objectInfo, BitStream& bsIn)` Called by the system when creating static objects receved from the server after 
connecting. This is an abstract factory method that needs to be defined. `typeID` coresponds to the object class to create, `objectInfo` contains system defined information used 
to create game objects, and therefore will not all be nessesary for static objects. `bsIn` is used for custom paramiters, and will be in the order data is put into it in 
`serialize(...)` for that class. It is important that you only read what is expected from `bsIn`, as to leave the read position at the end of the data for the object. The new 
object should be instantiated on the heap, with a pointer to it being returned.

`GameObject* gameObjectFactory(uint typeID, uint objectID, ObjectInfo& objectInfo, BitStream& bsIn)` Called by the system when the server creates a game object that needs to be 
syncronized. This is an abstract factory method that needs to be defined. Similar to `staticObjectFactory(...)`, except it creates game objects instead of static objects. 
Because clients make no distiction between game objects and client objects owned by other clients connected to the same server, this function also needs to be able to create 
custom client object classes, still returning a game object pointer.

`ClientObject* clientObjectFactory(uint typeID, ObjectInfo& objectInfo, BitStream& bsIn)` Called by the system to create the client object owned by this instance after 
connecting to a server. This is an abstract factory method that needs to be defined. Similar to `gameObjectFactory(...)`, except returns a client object pointer. No object ID is 
passed because the client ID should be used, which can be receved from `getClientID()`.

## Usage
A custom class needs to inherit from the client class, implementing `staticObjectFactory(...)`, `gameObjectFactory(...)`, and `clientObjectFactory(...)`, calling 
`systemUpdate()` regularly and passing packets to `processSystemMessage(...)`. On startup, `peerInterface` needs to be setup with `SetOccasionalPing(true)`.

With the server handling important game logic and processing input, the clients only purposes are collecting player input, drawing, and non-essential logic that doesnt 
nessesarily have to be syncronized.

The struct for input (`Input`) is given a default definition in *ClientObject.h* with some common input values, but it is encouraged to change to only contain what is needed.



# Shared
The contents of the shared project are used by both the server and clients, and include objects and custom message IDs, among other things. 
While custom shared classes should be kept in a seperate project from the client and server, it will enevitably be nessesary to interface with the server or client class, such 
as creating an object on the server in responce to a player input. In these cases there are multiple options, with which one to use depending on the situation.
- The simplest way is to include the server or client class directly. This can be suitable when logic is only used by the server or the client, but not both.
- When the same logic needs to be used by the server and clients, a beter solution is to leave the function undefined in the shared project, and instead define it within the 
server and client projects in a seperate .cpp file. The result of this is that the same function can be called in both projects with different functionality.

## Custom messages
The system adds some new messages on top of raknets. As such, when adding new messages, instead of starting from raknet's `ID_USER_PACKET_ENUM`, use `ID_USER_CUSTOM_ID` from 
*GameMessages.h* instead.


## Objects
typeID, serialize, colliders

### Static object

### Game object
collision events

### Client object
how input is expected to be used