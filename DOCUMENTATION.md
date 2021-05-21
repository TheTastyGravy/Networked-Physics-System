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
object destroying itself after hitting something). Note that the object will be destroyed at the end of a `systemUpdate()` call.

## Usage
A custom class needs to inherit from the server class, implementing `gameObjectFactory(...)` and `clientObjectFactory(...)`, calling `systemUpdate()` regularly and passing 
packets to `processSystemMessage(...)`. On startup, `peerInterface` needs to be setup with `SetOccasionalPing(true)`, and any static objects need to be created. The server uses 
a fixed time step for physics, which can be set using its constructor.

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
as creating an object on the server in responce to a player input. Its recomended to not define this functionality in the shared project, instead defining the function 
independantly for the Server and Client projects. This can be done by putting the function definition in a .cpp file within both projects. The result is both projects having the 
same function declaration in the shared header file, with different definitions.

## Custom messages
The system adds some new messages on top of raknets. As such, when adding new messages, instead of starting from raknet's `ID_USER_PACKET_ENUM`, use `ID_USER_CUSTOM_ID` from 
*GameMessages.h* instead.


## Objects
When creating a custom object class, it is important to assign `typeID` in its constructor to a unique value. It is used by factory methods to determine which class to use, so 
its important not to have multiple classes using the same ID.

The `serialize(BitStream)` method is used by the server when sending the object to be created on a client. It is important that the base function is called before any custom 
data is added to the bitstream, and this custom data will be passed to client factory methods.

Colliders are not nessesary, but an object will not be able to collide without it. Two colliders are avalable: Sphere and Oriented Boundiong Box (OBB) A pointer to a new 
instance should be passed to the base constructor, and the instance will be deleted in the base deconstructor.

### Static object
Static objects have no physics and can not move. Their only purpose is to be used for static geometry in a level. It needs to be created at startup on the server, and is only 
sent to clients once when they join, so if any objects are moved, they will become desyncronized across clients.

### Game object
Game objects derive from static objects, but have added physics and are syncronized across clients. All game objects have a unique object ID used to identify it in messages 
between client and server. Game objects are updated every tick on the server, and exist in the past on clients, with dead reckoning (with collisions) being used between server 
updates.

Variables such as mass, elasticity, drag, and friction should all be self explanitory. `lockRotation` is used to ignore angular velocity, effectivly locking the objects 
rotation.

`void onCollision(StaticObject* other, Vector3 contact, Vector3 normal)` is called after a collision is resolved with another object both on the server and clients.

`void fixedUpdate(float timeStep)` is called at the start of each physics step, including durring prediction, so it should not rely on external state.

### Client object
Client objects have physics like a game object, but are owned by a client and can respond to player input. They are created when a client connects to a server, and are destroied 
when they disconnect. The object ID of a client object is its owners client ID.

Input is processed in 2 ways, movement and action, with each having a seperate function.

Movement returns a physics state representing a change in state, or a difference, and is 
applied on top of the current state of the client object. Clients use this to predict player movement ahead of the server by processing input as it is receved, and when a server 
update is receved, all predictions are then re-applied on top of it. The server will also process movement, with the caveat that client objects are not kept in the present on 
the server, instead only being updated as input is receved. When input is being receved constantly this works without issue, but if input stops being sent, a large delta time 
can build up and lead to unintended results.

Actions are anything that does not affect the objects physics state, such as shooting. They are primarily processed by the server, but can also be used for prediction for 
clients.
