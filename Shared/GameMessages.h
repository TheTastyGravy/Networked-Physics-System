#pragma once
#include <MessageIdentifiers.h>

enum GameMessages
{
	ID_SERVER_SET_CLIENT_ID = ID_USER_PACKET_ENUM + 1,	// Used when client connects to send client ID and static objects
	ID_SERVER_CREATE_CLIENT_OBJECT,		// Used when client connects to create their client object

	ID_SERVER_CREATE_GAME_OBJECT,	// Used to instantiate a new game object
	ID_SERVER_DESTROY_GAME_OBJECT,	// Used to destroy a game object
	ID_SERVER_UPDATE_GAME_OBJECT,	// Used to update the rigidbody values of a game object

	ID_CLIENT_INPUT		// Used to send player input to the server
};