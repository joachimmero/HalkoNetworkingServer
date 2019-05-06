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

std::unordered_map<std::string, Room*> *rooms;


void shutdownServer(SOCKET ClientSocket)
{
	int iResult;
	//shutdown the send half of the connection since no more data will be sent
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		printf("shutdown failed: &d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}

	//cleanup
	closesocket(ClientSocket);
	//WSACleanup();
}

void passData(SOCKET* sender_sock, Room *room)
{

	while (*sender_sock != INVALID_SOCKET)
	{	
		unsigned int iResult = 0;
		int iSendResult = 0;
		int recvbuflen = DEFAULT_BUFLEN;
		char recvbuf[DEFAULT_BUFLEN];
		iResult = recv(*sender_sock, recvbuf, recvbuflen, 0);
		printf("Bytes received: %d\n", iResult);

		if (iResult > 0)
		{
			for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++) 
			{
				if (it->first != sender_sock)
				{
					SOCKET *receiver_sock = it->first;
				
					iSendResult = send(*receiver_sock, recvbuf, iResult + 4, 0);
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
					//printf("Bytes sent: %d\n", iSendResult);
				}
			}
		}
		//If there was an error receiving data from the sender_sock -> sender_sock left the server.
		else if (iResult == 0)
		{
			printf("Connection closed with client" + *sender_sock);
			for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++)
			{
				if (it->first != sender_sock)
				{
					SOCKET *receiver_sock = it->first;
					const char* leftclientid = (const char*)sender_sock;
					unsigned int len = 9;
					//Send all clients info that a player with the id of "leftclientid" has left the server.
					char buf[9];
					buf[0] = ((const char*)&len)[0];
					buf[1] = ((const char*)&len)[1];
					buf[2] = ((const char*)&len)[2];
					buf[3] = ((const char*)&len)[3];
					buf[4] = 'l';
					buf[5] = leftclientid[0];
					buf[6]  = leftclientid[1];
					buf[7] = leftclientid[2];
					buf[8] = leftclientid[3];

					iSendResult = send(*receiver_sock, buf, 9, 0);
				}
			}
			//Remove the client who left from the room-objects clients-list. If the number of the clients after this is 0, remove the room from the rooms container and delete the room object.
			if (room->RemoveClient(sender_sock) == 0)
			{
				rooms->erase(room->_name);
				delete room;
			}
			closesocket(*sender_sock);
			*sender_sock = INVALID_SOCKET;
		}
		else
		{
			printf("recf failed: %d\n", WSAGetLastError());
			closesocket(*sender_sock);
			WSACleanup();
			return;
		}	
	}
	printf("Thread stopped...");
	//Delete the socket;
	delete sender_sock;
}

unsigned int  createRoom(SOCKET *c, std::string name, std::string roomName, int roomSize)
{
	for (auto it = rooms->begin(); it != rooms->end(); it++)
	{
		if (it->first == roomName)
		{
			return 1;
		}
	}
	Room *room = new Room(roomName, roomSize);
	room->AddClient(std::make_pair(c, name));
	std::cout << room;
	rooms->insert({ roomName, room });
	const char* id = (const char*)c;
	unsigned int len = 5;
	char data[9] = { ((const char*)&len)[0], ((const char*)&len)[1], ((const char*)&len)[2], ((const char*)&len)[3], 'c', id[0], id[1], id[2], id[3] };
	int iSendResult1 = send(*c, data, 9, 0);
	passData(c, room);

	return 0;
}

unsigned int JoinRoom(SOCKET *c, std::string name, std::string roomName)
{
	bool roomfound = false;
	for (auto it = rooms->begin(); it != rooms->end(); it++)
	{
		if (it->first == roomName)
		{
			Room room = *it->second;
			if (room.GetClientCount() == room.maxPlayers)
			{
				return 2;
			}
			roomfound = true;
		}
	}
	if (!roomfound)
	{
		return 1;
	}
	Room *room = rooms->at(roomName);
	std::cout << rooms->at(roomName);
	room->AddClient(std::make_pair(c, name));
	const char* id = (const char*)c;
	unsigned int len = 5;
	//Send the client its id.
	char data[9] = { ((const char*)&len)[0], ((const char*)&len)[1], ((const char*)&len)[2], ((const char*)&len)[3], 'j', id[0], id[1], id[2], id[3] };
	//HANDLE ERROR
	int iSendResult1 = send(*c, data, 9, 0);

	//Get the number of clients currently in the room and minus by one.
	unsigned int players = (unsigned int)room->GetClients()->size() - 1;
	const char* cc = (const char*)&players;
	char ccdata[4] = { cc[0], cc[1], cc[2], cc[3] };
	//HANDLE ERROR
	int iSendResult2 = send(*c, ccdata, 4, 0);
	//Send info of all clients in the room to the client that joined it.
	for (auto it = room->GetClients()->begin(); it != room->GetClients()->end(); it++)
	{
		if (it->first != c)
		{
			int iSendResult3 = 0;
			const char* id = (const char*)it->first;
			unsigned int cnamelen = (unsigned int)it->second.length(); //The length of the clients name in bytes
			const char* lenbytes = (const char*)&cnamelen;
			char buf[100];
			//The length of the stream to be sent int bytes
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
			//HANDLE ERROR
			iSendResult3 = send(*c, buf, 8 + it->second.length(), 0);
			if (iSendResult3 == SOCKET_ERROR)
			{
				printf("send failed: %d\n", WSAGetLastError());
			}
		}
	}

	
	passData(c, room);
	
	/*if (room._clients.size() < room._size)
	{
		passData(client, &rooms->at(roomName));
		return "Joining room...";
	}
	else
	{
		return "Room full!";
	}*/
}

void StartHub(SOCKET c)
{
	int iResult = 0;
	int iSendResult = 0;
	SOCKET* client = new SOCKET;
	*client = c;
	//First element is to check whether the client wants to create or join a room.
	//The last element is the size of the room.
	//Everything in the middle is the name of the room.

	//Send a callback to the client, that client is now connected to the server.
	unsigned int len = 1;
	char callback[5] = { ((const char*)&len)[0], ((const char*)&len)[1], ((const char*)&len)[2], ((const char*)&len)[3], 's' };
	iSendResult = send(*client, callback, 5, 0);

	//Signed char eli -127 - 127
	char nxtbuflength[2];
	//Vastaanota clientilt� bittein� integer, joka m��rittelee kuinka pitk� seuraava streami tulee olemaan.
	recv(*client, nxtbuflength, 2, 0);
	int buflen = (int)nxtbuflength[0];
	int namelen = (int)nxtbuflength[1];
	
	unsigned int success;
	//[0] = 0 -> Create Room or 1 -> Join Room
	//[1] - [last - 1] Room Name
	//[last] Max players in room

	//Loop as long as the client hasn't succesfully created or joined a room.
	do
	{
		char recvbuf[127];
		iResult = recv(*client, recvbuf, buflen, 0);
		printf(recvbuf);
		if (iSendResult == SOCKET_ERROR)
		{
			//Do something about this error
			printf("Error: " + WSAGetLastError());
		}
		if (iResult == 0)
		{
			//Do something about this error
			printf("Error: " + WSAGetLastError());
		}
		else
		{
			//clientName-string
			std::string n;
			//roomName-string
			std::string r;
			int size = 0;
			for (int i = 1; i < buflen; i++)
			{
				if (i <= namelen)
				{
					n += recvbuf[i];
				}
				else if (i > namelen && i < buflen - 1)
				{
					r += recvbuf[i];
				}
				else if (i == buflen - 1)
				{
					if (recvbuf[0] == '0')
					{
						size = (int)recvbuf[i] - 48;
						success = createRoom(client, n, r, size);

						if (success == 1)
						{
							unsigned int len = 43;
							char buf[47] = {
								((const char*)&len)[0],
								((const char*)&len)[1],
								((const char*)&len)[2],
								((const char*)&len)[3],
								'f',
								'c',
								'A',' ','r','o','o','m',' ','w','i','t','h',' ','t','h','e',' ','s','a','m','e',' ','n','a','m','e',' ','a','l','r','e','a','d','y',' ','e','x','i','s','t','s','.'
							};

							send(c, buf, 47, 0);
						}
					}
					else if (recvbuf[0] == '1')
					{
						r += recvbuf[i];
						success = JoinRoom(client, n, r);
						
						if (success == 1)
						{
							unsigned int len = 17;
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
							send(c, buf, 21, 0);
						}
						else if (success == 2)
						{
							unsigned int len = 12;
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
							send(c, buf, 16, 0);
						}
					}
				}
			}
		}
	} while (success != 0);
}

int main() {

	WSADATA wsaData;
	int iResult;
	SOCKET ListenSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	const int roomSize = 2;

	//Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: &d\n", iResult);
		return 1;
	}

#define DEFAULT_PORT "27015"

	//T�ytt�� hintsin muistipaikan nollilla. Ensimm�inen parametri on osoitin
	//alkumuistipaikkaan. Toinen parametri on hintsin varaaman muistipaikan koko.
	ZeroMemory(&hints, sizeof(hints));
	//M��ritell��n servenin osoitteen tiedot. gettaddrinfo k�ytt�� t�t�.
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Resolve the local address and port to be used by the server
	//Muuntaa host nimest� osoitteen. Palauttaa hostin tiedot resultsiin.
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: &d\n", iResult);
		return 1;
	}

	//Create a SOCKET for the server to listen for client connections
	//T�m� socket on sidottu tiettyyn transport serviceen. -> T�ss� ohjelmassa TCP
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): &d\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	//Setup the TCP listening socket
	//T�ss� luoto socket "ListenSocket" sidotaan tiettyyn ip-osoitteeseen ja 
	//porttiin
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: &d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	//Vapautetaan resultsin muisti.
	freeaddrinfo(result);
	rooms = new std::unordered_map<std::string, Room*>();

	while (true)
	{
		SOCKET client = INVALID_SOCKET;
		//T�ss� kuunnellaan onko tulevia yhdist�mispyynt�j� serverille
		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			printf("Listen failed with error: &d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		//Check if the socket is an INVALID_SOCKET
		if (client = INVALID_SOCKET)
		{
			//Accept a client socket and add it to the ClientSockets array
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

		//Luodaan s�ie, jolla ClientSocketin kaksi  clientti� voivat 
		//keskustella kesken��n. Kutsutaan s�ikeell� receiveData funktiota
		//parametrill� ClientSockets
		std::thread(StartHub, client).detach();
	}
	return 0;
}

