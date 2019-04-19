#include <string>
#include <unordered_map>
#include <vector>
#include <ws2tcpip.h>

class Room
{
public:
	Room *p;
	int maxPlayers;

	Room(std::string name, int size);
	~Room();
	void AddClient(std::pair<SOCKET*, std::string>);
	void RemoveClient(SOCKET *client);
	std::unordered_map<SOCKET*, std::string>* GetClients();
private:
	void InformClients(std::pair<SOCKET*, std::string> newclient); 	//Inform all clients in the room, that a new client has connected.
	std::string _name;
	std::unordered_map<SOCKET*, std::string> *_clients; 
};