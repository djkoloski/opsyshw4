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
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

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
	
	char buffer[BUFFER_SIZE + 1];
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
			// Write over the newline with a null terminator
			buffer[n] = 0;
			cout << "[thread " << pthread_self() << "] Rcvd: " << buffer << endl;
			
			char *token = strtok(buffer, " \n");
			
			if (!strcmp(token, "READ"))
			{
				// TODO add errors if stuff is wrong
				token = strtok(NULL, " \n");
				string name = ".storage/" + string(token);
				token = strtok(NULL, " \n");
				int offset = atoi(token);
				token = strtok(NULL, " \n");
				int length = atoi(token);
				
				int byteStart = offset;
				int byteEnd = byteStart + length;
				while (byteStart < byteEnd)
				{
					char *fileData = new char[BUFFER_SIZE + 1];
					int len;
					
					stringstream stream;
					switch (data.frameManager.get(name, byteStart, byteEnd, fileData, len))
					{
						case 0:
							fileData[len] = 0;
							stream << "ACK " << len << "\n" << fileData << "\n";
							delete[] fileData;
							
							cout << "[thread " << pthread_self() << "] Sent: ACK " << len << endl;
							cout << "[thread " << pthread_self() << "] Transferred " << len << " bytes from offset " << byteStart << endl;
							break;
						case 1:
							stream << "ERROR: NO SUCH FILE" << endl;
							
							cout << "[thread " << pthread_self() << "] Sent: ERROR NO SUCH FILE" << endl;
							
							byteStart = byteEnd;
							break;
						case 2:
							stream << "ERROR: INVALID BYTE RANGE" << endl;
							
							cout << "[thread " << pthread_self() << "] Sent: ERROR INVALID BYTE RANGE" << endl;
							
							byteStart = byteEnd;
							break;
						default:
							stream << "ERROR: UNKNOWN ERROR" << endl;
							
							cout << "[thread " << pthread_self() << "] Sent: ERROR UNKNOWN ERROR" << endl;
							
							byteStart = byteEnd;
							break;
					}
					
					string message = stream.str();
					
					send(data.sockID, message.c_str(), message.size(), 0);
					
					byteStart += len;
				}
				
				if (length == 0)
					send(data.sockID, "ACK 0\n\n", 8, 0);
			}
			else if (!strcmp(token, "DELETE"))
			{
				// TODO add errors if stuff is wrong
				token = strtok(NULL, " \n");
				string fileName = string(token, strlen(token));
				string name = ".storage/" + fileName;
				
				stringstream stream;
				switch (data.frameManager.removeFile(name))
				{
					case 0:
						stream << "ACK" << endl;
						cout << "[thread " << pthread_self() << "] Deleted " << fileName << " file" << endl;
						cout << "[thread " << pthread_self() << "] Sent: ACK" << endl;
						break;
					case 1:
						stream << "ERROR: NO SUCH FILE" << endl;
						cout << "[thread " << pthread_self() << "] Sent: ERROR NO SUCH FILE" << endl;
						break;
					default:
						stream << "ERROR: UNKNOWN ERROR" << endl;
						cout << "[thread " << pthread_self() << "] Sent: ERROR UNKNOWN ERROR" << endl;
						break;
				}
				
				string message = stream.str();
				
				send(data.sockID, message.c_str(), message.size(), 0);
			}
			else if (!strcmp(token, "DIR"))
			{
				stringstream stream;
				int fileCount = 0;
				DIR *dir;
				struct dirent *ent;
				if ((dir = opendir(".storage")) != NULL)
				{
					while ((ent = readdir(dir)) != NULL)
					{
						if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
							continue;
						
						++fileCount;
						stream << ent->d_name << endl;
					}
					closedir(dir);
					
					string files = stream.str();
					stream.str("");
					stream << fileCount << endl << files;
				}
				else
					stream << "ERROR: STORAGE DIRECTORY BROKEN" << endl;
				
				string message = stream.str();
				
				send(data.sockID, message.c_str(), message.size(), 0);
			}
			else if (!strcmp(token, "STORE") || !strcmp(token, "ADD"))
			{
				// TODO: Add error messages for invalid arguments
				stringstream stream;
				token = strtok(NULL, " \n");
				string name = ".storage/" + string(token);
				token = strtok(NULL, " \n");
				int bytes = atoi(token);
				
				char *tail = token + strlen(token) + 1;
				
				struct stat buf;
				int status = lstat(name.c_str(), &buf);
				
				if (status == 0)
				{
					stream << "ERROR: FILE EXISTS" << endl;
					cout << "[thread " << pthread_self() << "] Sent: ERROR FILE EXISTS" << endl;
				}
				else
				{
					data.frameManager.lockFile(name);
					
					FILE *f = fopen(name.c_str(), "wb");
					cout << "Tail: " << tail << endl;
					cout << (unsigned long)tail << " : " << (unsigned long)buffer << endl;
					int tailBytes = n - (tail - buffer);
					cout << "Header was " << (tail - buffer) << " bytes long" << endl;
					cout << "Got " << tailBytes << " tail bytes" << endl;
					fwrite(tail, sizeof(char), tailBytes, f);
					
					if (n != 0)
					{
						stream << "ACK" << endl;
						cout << "[thread " << pthread_self() << "] Transferred file (" << bytes << " bytes)" << endl;
						cout << "[thread " << pthread_self() << "] Sent: ACK" << endl;
					}
					fclose(f);
					
					data.frameManager.unlockFile(name);
				}
				
				string message = stream.str();
				
				send(data.sockID, message.c_str(), message.size(), 0);
			}
		}
	} while (n > 0);
	
	close(data.sockID);
	pthread_exit(NULL);
	
	return NULL;
}

int main(int argc, char **argv)
{
	unsigned short port = 8765;
	
	for (int i = 0; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-p"))
		{
			port = atoi(argv[i + 1]);
			++i;
		}
	}
	
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
