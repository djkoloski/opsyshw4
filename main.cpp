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

#include "framemanager.h"

#define BUFFER_SIZE 1024

using namespace std;
using namespace hw4;

struct threadData {
	int sockID;
};

//Client thread
void *ClientThread(void *arg) {
	threadData data = *(threadData *)arg;
	free(arg);
	char buffer[BUFFER_SIZE];
	int n;
	do {
		n = recv(data.sockID, buffer, BUFFER_SIZE, 0);
		//Socket unable to receive from
		if (n < 0) {
			cerr << "Failed to receive from socket" << endl;
		}
		//Socket closed
		else if (n == 0) {
			cout << "[thread " << pthread_self() << "] Client closed its socket....terminating" << endl;
		}
		//Message receive
		else {
			cout << "[thread " << pthread_self() << "] A message was received!" << endl;
		}
	} while (n > 0);
	close(newsock);
	pthread_exit(&pthread_self());
	return NULL;
}

int main() {
	FrameManager frameManager;
	
	int byteStart = 1;
	int byteEnd = 1024 * 40 + 1;
	while (byteStart < byteEnd)
	{
		char *data;
		int length;
		switch (frameManager.get(".storage/abc.txt", byteStart, byteEnd, data, length))
		{
			case 0:
				printf("Got %i bytes\n", length);
				delete[] data;
				byteStart += length;
				break;
			case 1:
				printf("No such file!\n");
				byteEnd = -1;
				break;
			case 2:
				printf("Invalid byte range\n");
				byteEnd = -1;
				break;
			default:
				break;
		}
	}
	
	//Create a listener socket
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		cerr << "Failed to create a listener socket. Exiting..." << endl;
		exit(EXIT_FAILURE);
	}
	//Socket address struct
	struct sockaddr_in server;
	server.sin_family = PF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	//Assign port
	unsigned short port = 8765;
	server.sin_port = htons(port);
	//Bind socket to address
	int serverSize = sizeof(server);
	if (bind(sock, (struct sockaddr *)&server, serverSize) < 0) {
		cerr << "Failed to bind socket. Exiting..." << endl;
		exit(EXIT_FAILURE);
	}
	//Listen for up to SOMAXCONN clients
	listen(sock, 128);
	cout << "Listening on port " << port << endl;

	struct sockaddr_in client;
	int clientSize = sizeof(client);

	int tid;

	while (1) {
		int newsock = accept(sock, (struct sockaddr *)&client, (socklen_t*)&clientSize);
		//Get hostname
		char hostName[NI_MAXHOST];
		struct in_addr ipv4addr;
		inet_pton(AF_INET, inet_ntoa(client.sin_addr), &ipv4addr);
		if (getnameinfo((const sockaddr *)&client, clientSize, hostName, sizeof(hostName), NULL, 0, NI_NAMEREQD) != 0) {
			cout << "Received incoming connection from unresolved host" << endl;
		}
		else {
			cout << "Received incoming connection from " << hostName << endl;
		}
		//Thread the new socket
		pthread_t id;
		threadData arg;
		arg.sockID = newsock;
		tid = pthread_create(&id, NULL, ClientThread, arg);
		//Client thread doesn't exist
		if (tid == 0) {
			cerr << "Could not create client thread. Exiting..." << endl;
			exit(EXIT_FAILURE);
		}
		//Server
		close(newsock);
	}
	close(sock);
	
	return EXIT_SUCCESS;
}
