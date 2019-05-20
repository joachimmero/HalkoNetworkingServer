#include "room.h"
#include <winsock2.h>
#include <thread>
#include <unordered_map>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

std::unordered_map<std::string, Room*> *rooms;

int checkRecvErrors(int iResult, SOCKET* client)
{
	if (iResult == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		switch (error)
		{
		case WSAETIMEDOUT:
		case WSAECONNRESET:
		case WSAECONNABORTED:
			std::cout
				<< "Connection lost with client: "
				<< *client
				<< ". Closing socket...\n"
				<< error;
			break;
		}

		//Close the socket with which the connection was lost.
		closesocket(*client);
		*client = INVALID_SOCKET;
		//return 0 to indicate that an error occured
		return 0;
	}
	else if (iResult == 0)
	{
		printf("Error: " + WSAGetLastError());
		std::cout
			<< "Connection closed with client: "
			<< *client
			<< std::endl;
		//Close the socket with which the connection was lost.
		closesocket(*client);
		*client = INVALID_SOCKET;
		//return 0 to indicate that an error occured
		return 0;
	}
	return 1;
}

void passData(SOCKET* sender_sock, Room *room)
{
	bool clientinroom = true;
	//Receive and send data while the client socket is not INVALID_SOCKET
	while (clientinroom)
	{	
		unsigned int iResult = 0;
		unsigned int iSendResult = 0;
		unsigned int recvbuflen = DEFAULT_BUFLEN;
		//Create a buffer for receiving data
		char recvbuf[DEFAULT_BUFLEN];
		//Receive data from the client
		iResult = recv(*sender_sock, recvbuf, recvbuflen, 0);
		//Check if data was received succesfully.
		if (iResult > 0)
		{
			if (recvbuf[0] != 'l')
			{
				//Go through all the clients in the room.
				for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++)
				{
					//Check that the client isn't sender_sock.
					if (it->first != sender_sock)
					{
						//Create a socket for the receiving client 
						//and copy it->first to it.
						SOCKET *receiver_sock = it->first;
						//Send the received data to the receiving client.
						iSendResult = send(*receiver_sock, recvbuf, iResult, 0);
						if (iSendResult == SOCKET_ERROR)
						{
							printf("send failed: %d\n", WSAGetLastError());
							if (WSAGetLastError() == 10057)
							{
								*receiver_sock = INVALID_SOCKET;
								printf("Connection lost with client: " + *receiver_sock);
								//iSendResult = send(*sender_sock, "Connection lost with other client", 28, 0);
							}
						}
					}
				}
			}
			//If the client sent info that he left the room intentionally.
			else if(recvbuf[0] == 'l')
			{
				std::cout 
					<< "Client " 
					<< *sender_sock
					<< " left the room\n";

				//Remove the client who left from the room-objects clients-list. If the number of the clients after this is 0, remove the room from the rooms container and delete the room object.
				if (room->RemoveClient(sender_sock) == 0)
				{
					rooms->erase(room->_name);
					delete room;
					room = nullptr;
				}

				clientinroom = false;

				//Send a callback to the client that left the room.
				send(*sender_sock, "l", 1, 0);

				if (room != nullptr)
				{
					//Go through all the clients in the room.
					for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++)
					{
						//Check that the client isn't sender_sock
						if (it->first != sender_sock)
						{
							//Create a socket for the receiving client 
							//and copy it->first to it.
							SOCKET *receiver_sock = it->first;
							//Convert the id of the client that left in bytes.
							const char* leftclientid = (const char*)sender_sock;
							//Create a buffer holding the info of 
							//the client that left the room.
							char buf[9];
							buf[0] = 9;
							buf[1] = 0;
							buf[2] = 0;
							buf[3] = 0;
							buf[4] = 'l';
							buf[5] = leftclientid[0];
							buf[6] = leftclientid[1];
							buf[7] = leftclientid[2];
							buf[8] = leftclientid[3];
							//Send the buffer to the client.
							iSendResult = send(*receiver_sock, buf, 9, 0);
						}
					}
				}
			}
		}
		//If there was an error receiving data from the sender_sock -> sender_sock left the server.
		else if (iResult == 0)
		{
			printf("Connection closed with client" + *sender_sock);
			//Go through all the clients in the room.
			for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++)
			{
				//Check that the client isn't sender_sock
				if (it->first != sender_sock)
				{
					//Create a socket for the receiving client 
					//and copy it->first to it.
					SOCKET *receiver_sock = it->first;
					//Convert the id of the client that left in bytes.
					const char* leftclientid = (const char*)sender_sock;
					//Create a buffer holding the info of 
					//the client that left the room.
					char buf[9];
					buf[0] = 9;
					buf[1] = 0;
					buf[2] = 0;
					buf[3] = 0;
					buf[4] = 'l';
					buf[5] = leftclientid[0];
					buf[6]  = leftclientid[1];
					buf[7] = leftclientid[2];
					buf[8] = leftclientid[3];
					//Send the buffer to the client.
					iSendResult = send(*receiver_sock, buf, 9, 0);
				}
			}
			//Remove the client who left from the room-objects clients-list. If the number of the clients after this is 0, remove the room from the rooms container and delete the room object.
			if (room->RemoveClient(sender_sock) == 0)
			{
				rooms->erase(room->_name);
				delete room;
			}
			//Close the socket that the client used.
			closesocket(*sender_sock);
			*sender_sock = INVALID_SOCKET;

			clientinroom = false;
		}
		else if (iResult == SOCKET_ERROR)
		{

		}
		else
		{
			printf("recf failed: %d\n", WSAGetLastError());
			closesocket(*sender_sock);
			WSACleanup();
			return;
		}	
	}
}

unsigned int  createRoom(SOCKET *c, std::string name, std::string roomName, unsigned int roomSize)
{
	//Go through all the rooms on the server
	//and check if a room with the name "name"
	//exists already.
	for (auto it = rooms->begin(); it != rooms->end(); it++)
	{
		if (it->first == roomName)
		{
			return 1;
		}
	}
	//Create a room object with a name and maximum player size.
	Room *room = new Room(roomName, roomSize);
	//Creata a pair from the clients socket and the players name.
	//and add it to the rooms client -container.
	room->AddClient(std::make_pair(c, name));
	//Add the roomName and the room-object as a pair to
	//the rooms-unordered_map.
	rooms->insert({ roomName, room });
	//Make a character pointer and point it to the SOCKET c
	//to get the id of the client in bytes.
	const char* id = (const char*)c;
	//Create a callback buffer that holds the information of its length,
	//the flag c ("Created a room") and the players id.
	const char callback[9] = { 5, 0, 0, 0, 'c', id[0], id[1], id[2], id[3] };
	//Send the buffer to the client.
	int iSendResult1 = send(*c, callback, 9, 0);
	//"Enter the room"
	//Start receiving and passing data to other clients
	//that are connected to the same room.
	passData(c, room);

	return 0;
}

unsigned int JoinRoom(SOCKET *c, std::string name, std::string roomName)
{
	//Create a boolean for indicating if 
	//the room "roomName" exists.
	bool roomfound = false;
	//Go through all the rooms on the server.
	for (auto it = rooms->begin(); it != rooms->end(); it++)
	{
		//Check if the room was found
		if (it->first == roomName)
		{
			//Check if the room is full.
			Room room = *it->second;
			if (room.GetClientCount() == room.maxplayers)
			{
				//If the room was full, return 2;
				return 2;
			}
			roomfound = true;
		}
	}
	//If the room wasn't found return 1.
	if (!roomfound)
	{
		return 1;
	}
	//Create a room object and copy the room "roomName" to it.
	Room *room = rooms->at(roomName);
	//Add the joined client to the room.
	room->AddClient(std::make_pair(c, name));
	//Make a character pointer and point it to the SOCKET c
	//to get the id of the client in bytes.
	const char* id = (const char*)c;
	//Create a callback buffer that holds the information of its length,
	//the flag j ("Joined a room") and the players id.
	char data[9] = { 5, 0, 0, 0, 'j', id[0], id[1], id[2], id[3] };
	//Send the callback to the client
	int iSendResult1 = send(*c, data, 9, 0);

	//Get the number of clients currently in the room and subtract it by one.
	unsigned int players = (unsigned int)room->GetClients()->size() - 1;
	//Create a buffer that holds information of 
	//the amount of clients currently in the room
	char playercountbuf[4] = { ((char*)&players)[0], ((char*)&players)[1], ((char*)&players)[2], ((char*)&players)[3] };
	//Send the buffer to the client.
	int iSendResult2 = send(*c, playercountbuf, 4, 0);
	//Send info of all clients in the room to the client that joined it.
	for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++)
	{
		//If the client isn't the client that joined the room.
		if (it->first != c)
		{
			int iSendResult3 = 0;
			//Get the id of the client in bytes.
			const char* id = (const char*)it->first;
			unsigned int namelen = (unsigned int)it->second.length(); 
			//The length of the clients name in bytes.
			const char* lenbytes = (const char*)&namelen;
			//Create a buffer that holds information of 
			//the length of the clients name, the id of the client
			//and the clients name.
			char buf[100];
			buf[0] = lenbytes[0];
			buf[1] = lenbytes[1];
			buf[2] = lenbytes[2];
			buf[3] = lenbytes[3];
			buf[4] = id[0];
			buf[5] = id[1];
			buf[6] = id[2];
			buf[7] = id[3];
			for (int i = 0; i < it->second.length(); i++)
			{
				buf[8 + i] = it->second.at(i);
			}
			//Send the buffer to the client.
			iSendResult3 = send(*c, buf, 8 + it->second.length(), 0);
			if (iSendResult3 == SOCKET_ERROR)
			{
				printf("send failed: %d\n", WSAGetLastError());
			}
		}
	}
	//"Enter the room"
	//Start receiving and passing data to other clients
	//that are connected to the same room.
	passData(c, room);

	return 0;
}

//Handle creation of rooms, joining to rooms
//and fetching a list of the rooms on the server.
void StartHub(SOCKET c)
{
	int iResult = 0;
	int iSendResult = 0;
	//Create a new SOCKET pointer and copy the info of "c" to it.
	SOCKET* client = new SOCKET;
	*client = c;
	//First element is to check whether the client wants to create or join a room.
	//The last element is the size of the room.
	//Everything in the middle is the name of the room.

	//Create a character array for the server connection callback.
		//The first four elements are a unsigned int of the value 1 in bytes,
		//and they represent the length of the callback-array - the first four bytes.
		//The last byte is a flag that indicates that the connection to the server was succesfull.
	char callback[5] = { 1, 0, 0, 0, 's' };

	//Send a callback to the client, that indicates a succesfull connection to the server.
	send(*client, callback, 5, 0);

	unsigned int success;
	//[0] = 0 -> Create Room or 1 -> Join Room
	//[1] - [last - 1] Room Name
	//[last] Max players in room

	//Loop as long as the client hasn't succesfully created or joined a room.
	do
	{
		//Signed char eli -127 - 127
		char nextbuflen[4];
		//Vastaanota clientiltä bitteinä integer, joka määrittelee kuinka pitkä seuraava streami tulee olemaan.
		iResult = recv(*client, nextbuflen, 4, 0);
		//Check if the recv-function returned an error.
		success = checkRecvErrors(iResult, client);
		if (success != 0)
		{
			success = 0;
			//TEMP: muuta -> unsigned int* buflen = (unsigned int*)&nextbuflen;
			//This is the length of the next stream to be received.
			unsigned int* buflen = (unsigned int*)&nextbuflen[0];
			//First element is a flag (0 = Create Room, 1 = Join Room, 2 = Get room list.
			//If flag = 0 or 1, elements 1...4 are the length of the clients name in bytes.
			//If flag = 0 or 1 elements 5...namelen - 1 are the clients name in characters.
			//If flag = 0 or 1 elements namelen...buflen - 2 are the rooms name in characters.
			//If flag = 0 the last element is the maximum number of players in the room.
			char recvbuf[127];
			iResult = recv(*client, recvbuf, *buflen, 0);
			//Check if the recv-function returned an error.
			success = checkRecvErrors(iResult, client);
			if (success != 0)
			{
				success = 0;

				if (recvbuf[0] == 'g')
				{
					//Check the amount of rooms on the server and
					//convert it to a const char (byte) -array.
					unsigned int count = (unsigned int)rooms->size();
					const char countbuf[4] = {
						((const char*)&count)[0],
						((const char*)&count)[1],
						((const char*)&count)[2],
						((const char*)&count)[3]
					};

					//Send the amount of rooms to the client in bytes.
					send(*client, countbuf, 4, 0);

					//Send all the rooms to the client.
					for (auto it = rooms->begin(); it != rooms->end(); it++)
					{
						//Get the information of a room.
						std::string roomname = it->first;
						unsigned int playerscount = it->second->GetClientCount();
						unsigned int maxplayers = it->second->maxplayers;

						//The length of the roomname in bytes.
						//This is sent so that the client knows the length of the next buffer.
						unsigned int namelen = (unsigned int)roomname.length();
						char buflen[4] = {
							((char*)&namelen)[0],
							((char*)&namelen)[1],
							((char*)&namelen)[2],
							((char*)&namelen)[3]
						};
						send(*client, buflen, 4, 0);


						//TEMP: Change the size of buf to what the real max size is.
						char buf[50] = {
							((char*)&playerscount)[0],
							((char*)&playerscount)[1],
							((char*)&playerscount)[2],
							((char*)&playerscount)[3],
							((char*)&maxplayers)[0],
							((char*)&maxplayers)[1],
							((char*)&maxplayers)[2],
							((char*)&maxplayers)[3]
						};
						for (unsigned int i = 8; i < 8 + namelen; i++)
						{
							int j = i - 8;
							buf[i] = roomname.at(j);
						}

						send(*client, buf, 8 + namelen, 0);
					}
					success = 1;
				}
				else
				{
					//Receive info about how long the name of the client is.
					char namelenbuf[4];
					//recv(*client, namelenbuf, 4, 0);
					unsigned int* namelen = (unsigned int*)&recvbuf[1];

					//clientName-string
					std::string n;
					//roomName-string
					std::string r;
					unsigned int size = 0;

					//Get all information from the stream. 
					//Start from index 5, because the first index determines whether
					//the player is creating ('0') or joining ('1') a room,
					//and the next four elements determine the length of the clients name.

					//Get client name from the stream
					for (unsigned int i = 5; i <= 5 + *namelen; i++)
					{
						n += recvbuf[i];
					}
					for (unsigned int i = 5 + *namelen; i < *buflen - 1; i++)
					{
						r += recvbuf[i];
					}
					if (recvbuf[0] == 'c')
					{
						//Cast the last element of the buffer(numeric character=
						//to an unsigned int.
						size = (unsigned int)recvbuf[*buflen - 1] - 48;
						//Check if a room with the name n can be created.
						//Returns 1 if room couldn't be created
						//Else returns 0.
						success = createRoom(client, n, r, size);

						if (success == 1)
						{
							unsigned int len = 43;
							//Create a callback buffer.
							//First element is a flag to indicate that something failed ("f").
							//Second element is a flag to indicate that it was room creation that failed ("c")
							//Rest is the callback message.
							char buf[47] = {
								((const char*)&len)[0],
								((const char*)&len)[1],
								((const char*)&len)[2],
								((const char*)&len)[3],
								'f',
								'c',
								'A',' ','r','o','o','m',' ','w','i','t','h',' ','t','h','e',' ','s','a','m','e',' ','n','a','m','e',' ','a','l','r','e','a','d','y',' ','e','x','i','s','t','s','.'
							};

							//Send the callback to the client.
							send(c, buf, 47, 0);
						}
					}
					else if (recvbuf[0] == 'j')
					{
						r += recvbuf[*buflen - 1];
						//Check if a room with the name n can be joined.
						//Returns 1 if room couldn't be joined, because it couldn't be found.
						//Returns 2 if room couldn't be joined, because it was full.
						//Else returns 0.
						success = JoinRoom(client, n, r);

						if (success == 1)
						{
							unsigned int len = 17;
							//Create a callback buffer.
							//First element is a flag to indicate that something failed ("f").
							//Second element is a flag to indicate that it was joining a room that failed ("j")
							//Rest is the callback message.
							char buf[21] =
							{
								((const char*)&len)[0],
								((const char*)&len)[1],
								((const char*)&len)[2],
								((const char*)&len)[3],
								'f',
								'j',
								'R','o','o','m',' ','n','o','t',' ','f','o','u','n','d','!'
							};
							//Send the callback to the client.
							send(c, buf, 21, 0);
						}
						else if (success == 2)
						{
							unsigned int len = 12;
							//Create a callback buffer.
							//First element is a flag to indicate that something failed ("f").
							//Second element is a flag to indicate that it was joining a room that failed ("j")
							//Rest is the callback message.
							char buf[16] =
							{
								((const char*)&len)[0],
								((const char*)&len)[1],
								((const char*)&len)[2],
								((const char*)&len)[3],
								'f',
								'j',
								'R','o','o','m',' ','f','u','l','l','!'
							};
							//Send the callback to the client.
							send(c, buf, 16, 0);
						}
					}
				}
			}
		}	
	} while (*client != INVALID_SOCKET);
	delete client;
}

int main() {

	WSADATA wsaData;
	int iResult;
	SOCKET ListenSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL, *ptr = NULL, hints;

	//Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: &d\n", iResult);
		return 1;
	}

	//Fill the memory used by "hints", with zero's.
	ZeroMemory(&hints, sizeof(hints));
	//Specify the servers address info.
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: &d\n", iResult);
		return 1;
	}

	//Create a SOCKET for the server to listen for client connections
	//This socket is bound to a specific transport service. In this program it's TCP.
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): &d\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	//Setup the TCP listening socket
	//Bind the "ListenSocket" to a specific ip-address and port.
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: &d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	//Release the memory of results.
	freeaddrinfo(result);

	//Create an unordered_map for storing the rooms that clients create.
	rooms = new std::unordered_map<std::string, Room*>();

	//Start a loop for listening to client connections.
	while (true)
	{
		SOCKET client = INVALID_SOCKET;
		//Place the "ListenSocket" in a listening-state.
		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			printf("Listen failed with error: &d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		//Check if the socket is an INVALID_SOCKET (empty)
		if (client = INVALID_SOCKET)
		{
			//Accept a client socket.
			client = accept(ListenSocket, NULL, NULL);

			//Check if client acception failed
			if (client == INVALID_SOCKET)
			{
				printf("accept failed: &d\n", WSAGetLastError());
				closesocket(ListenSocket);
				WSACleanup();
				return 1;
			}
		}

		//Create a thread where the socket can receive and send data.
		std::thread(StartHub, client).detach();
	}
	return 0;
}

