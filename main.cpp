/*Main*/

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <string>

#include "framemanager.h"

#define BUFFER_SIZE 1024

using namespace std;
using namespace hw4;

class ThreadData
{
	public:
		int sockID;
		FrameManager &frameManager;
		
		ThreadData(int s, FrameManager &f) :
			sockID(s),
			frameManager(f)
		{ }
};

//Client thread
void *ClientThread(void *arg)
{
	ThreadData data = *(ThreadData *)arg;
	delete[] (ThreadData *)arg;
	
	char buffer[BUFFER_SIZE];
	int n;
	do {
		n = recv(data.sockID, buffer, BUFFER_SIZE, 0);
		//Socket unable to receive from
		if (n == -1)
			cerr << "[thread " << pthread_self() << "] Encountered an error while waiting for client data" << endl;
		//Socket closed
		else if (n == 0)
			cout << "[thread " << pthread_self() << "] Client closed its socket....terminating" << endl;
		//Message receive
		else
		{
			char *token = strtok(buffer, " ");
			
			if (!strcmp(token, "READ"))
			{
				token = strtok(NULL, " ");
				string name = ".storage/" + string(token);
				token = strtok(NULL, " ");
				// TODO add errors if stuff is wrong
				int offset = atoi(token);
				token = strtok(NULL, "\n");
				int length = atoi(token);
				
				char *rest = strchr(token, 0) + 1;
				
				cout << "[thread " << pthread_self() << "] READ " << length  << " bytes from " << offset << " bytes into " << name << ": '" << rest << "'!" << endl;
			}
		}
	} while (n > 0);
	
	close(data.sockID);
	pthread_exit(NULL);
	
	return NULL;
}

int main()
{
	FrameManager frameManager;
	
	//Create a listener socket
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		cerr << "Failed to create a listener socket. Exiting..." << endl;
		exit(EXIT_FAILURE);
	}
	
	//Socket address struct
	struct sockaddr_in server;
	server.sin_family = PF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	unsigned short port = 8765;
	server.sin_port = htons(port);
	
	//Bind socket to address
	int serverSize = sizeof(server);
	if (bind(sock, (struct sockaddr *)&server, serverSize) < 0)
	{
		cerr << "Failed to bind socket. Exiting..." << endl;
		exit(EXIT_FAILURE);
	}
	
	//Listen for up to SOMAXCONN clients
	listen(sock, 128);
	cout << "Listening on port " << port << endl;

	struct sockaddr_in client;
	int clientSize = sizeof(client);

	int status;

	while (true)
	{
		int newsock = accept(sock, (struct sockaddr *)&client, (socklen_t*)&clientSize);
		
		//Get hostname
		char hostName[NI_MAXHOST];
		struct in_addr ipv4addr;
		inet_pton(AF_INET, inet_ntoa(client.sin_addr), &ipv4addr);
		
		if (getnameinfo((const sockaddr *)&client, clientSize, hostName, sizeof(hostName), NULL, 0, NI_NAMEREQD) != 0)
			cout << "Received incoming connection from unresolved host" << endl;
		else
			cout << "Received incoming connection from " << hostName << endl;
		
		//Thread the new socket
		pthread_t id;
		ThreadData *arg = new ThreadData(newsock, frameManager);
		status = pthread_create(&id, NULL, ClientThread, arg);
		
		//Client thread doesn't exist
		if (status != 0)
		{
			cerr << "Could not create client thread. Exiting..." << endl;
			exit(EXIT_FAILURE);
		}
	}
	close(sock);
	
	return EXIT_SUCCESS;
}
