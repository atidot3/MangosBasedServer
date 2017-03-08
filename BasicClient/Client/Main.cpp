#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <string>
#include <sstream>

#include "../../src/Shared/Define.h"
#include "../../src/authserver/AuthCodes.h"
#include "../../src/Shared/Util/WorldPacket.h"
#include "../../src/game/Server/Opcodes.h"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "8085"

#pragma pack(push,1)
typedef struct LOGIN
{
	uint8 cmd;
	std::string		username;
	std::string		password;
}LOGIN;
#pragma pack(pop)
#pragma pack(push,1)
struct ServerPktHeader
{
	uint16 size;
	uint16 cmd;
};
struct ClientPktHeader
{
	uint16 size;
	uint32 cmd;
};
#pragma pack(pop)
bool running = true;

void process_thread()
{
	
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char *sendbuf = "this is a test";
	char recvbuf[DEFAULT_BUFLEN] = "0";
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		running = false;
		return;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			running = false;
			return;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	/*if (ConnectSocket == INVALID_SOCKET)
	{
	printf("Unable to connect to server!\n");
	WSACleanup();
	return;
	}*/
	std::string user = "player";
	std::string pass = "pl";

	WorldPacket packet(CMSG_AUTH_SESSION, user.size() + pass.size() + 4);
	packet << user;
	packet << pass;
	packet << uint32(1);
	packet << uint32(0);

	ClientPktHeader clientheader;

	clientheader.cmd = packet.GetOpcode();
	EndianConvert(clientheader.cmd);
	clientheader.size = static_cast<uint16>(packet.size() + 2);
	EndianConvertReverse(clientheader.size);

	send(ConnectSocket, (reinterpret_cast<const char *>(&clientheader)), sizeof(ClientPktHeader), 0);
	send(ConnectSocket, (reinterpret_cast<const char *>(packet.contents())), packet.size(), 0);

	LOGIN stru;

	stru.cmd = CMD_AUTH_LOGIN;
	stru.password = "player";
	stru.username = "player";
	int i = 0;
	// Receive until the peer closes the connection
	while (running == true)
	{
		Sleep(10000);
		/*if (i >= 100)
			i = 0;
		send(ConnectSocket, (char*)&stru, sizeof(LOGIN), 0);
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
			uint8 cmd = (uint8)*recvbuf;
			printf("recv cmd: %d\n", cmd);
		}
		else if (iResult == 0)
		{
			printf("Connection closed\n");
		}
		else
		{
			printf("recv failed with error: %d\n", WSAGetLastError());
			running = false;
		}
		i++;*/
	}
	// cleanup
	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		running = false;
		return;
	}
	closesocket(ConnectSocket);
}

int __cdecl main(int argc, char **argv)
{
	WSADATA wsaData;
	std::string input = "";
	
	std::thread t[10000];
	
	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		running = false;
		return -1;
	}

	std::cout << "Please enter a valid sentence (with spaces):\n>";
	getline(std::cin, input);
	std::cout << "You entered: " << input << std::endl << std::endl;
	//Launch a group of threads
	for (int i = 0; i < atoi(input.c_str()); ++i)
	{
		t[i] = std::thread(process_thread);
	}
	std::cout << "Launched from the main\n";
	do
	{
		Sleep(100);
	} while (running == true);
	//Join the threads with the main thread
	for (int i = 0; i < atoi(input.c_str()); ++i)
	{
		t[i].join();
	}
	WSACleanup();
	return 0;
}
