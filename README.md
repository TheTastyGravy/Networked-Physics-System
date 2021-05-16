# Networked Physics System
A C++ system used to syncronize physics state between clients using an authoritative server, with client side prediction for player input. Uses RakNet for networking and Raylib 
with a cpp wrapper for math functions.

The system consists of three parts: the server, the client, and shared classes. Each of them have a project and compile to seperate static libraries. Refer to `DOCUMENTATION.md` 
for information about how the system works and implementation.

The system uses a custom 3D physics simulation that is prone to bugs resulting from angular velocity being calculated incorrectly from collisions involving objects rotated on 
multiple axes. It also only has two colliders: sphere and OBB.
