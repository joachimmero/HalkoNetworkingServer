#include "room.h"

Room::Room(std::string name, int size)
{
	Room::maxPlayers = size;
	Room::_name = name;
	_clients = new std::unordered_map<SOCKET*, std::string>();
	p = this;
}
Room::~Room()
{
	
}
void Room::AddClient(std::pair<SOCKET*, std::string> client)
{
	Room::_clients->insert(client);
	InformClients(client);
}
void Room::RemoveClient(SOCKET *client)
{
	Room::_clients->erase(client);
}
std::unordered_map<SOCKET*, std::string>* Room::GetClients()
{
	return _clients;
}
void Room::InformClients(std::pair<SOCKET*, std::string> client)
{
	SOCKET* c = std::get<SOCKET*>(client);
	std::string* n = &std::get<std::string>(client);

	if (_clients->size() > 1)
	{
		for (auto it = _clients->begin(); it != _clients->end(); it++)
		{
			if (it->first != c)
			{
				int iSendResult = 0;
				const char* id = (const char*)c;
				unsigned int datalen = (unsigned int)n->length() + 5;
				const char* datalenbytes = (const char*)&datalen;
				char buf[100];
				buf[0] = datalenbytes[0];
				buf[1] = datalenbytes[1];
				buf[2] = datalenbytes[2];
				buf[3] = datalenbytes[3];
				buf[4] = (char)'n';
				buf[5] = id[0];
				buf[6] = id[1];
				buf[7] = id[2];
				buf[8] = id[3];
				for (int i = 0; i < n->length(); i++)
				{
					buf[9 + i] = n->at(i);
				}
				iSendResult = send(*it->first, buf, 9 + n->length(), 0);
				if (iSendResult == SOCKET_ERROR)
				{
					printf("send failed: %d\n", WSAGetLastError());
				}
				//delete[] buf;
			}
		}
	}
}