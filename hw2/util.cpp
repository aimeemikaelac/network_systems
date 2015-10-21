/*
 * util.c
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#include "util.h"

using namespace std;

int getSocket(int port){
	int socket_fd, rc;
	struct sockaddr_in server_addr;
	char service[100];
	sprintf(service, "%i", port);

	//setup struct to create socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(seargsrvicePort);

	cout << "Running server" << endl;

	//request socket from system
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		return socket_fd;
	}

	//set socket reusable from previous program run
	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	//bind to socket
	status = bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0){
		cout << "Error binding on socket" << endl;
		return status;
	} else{
		cout << "Running HTTP server on TCP port: "<<servicePort<<endl;
	}
	MSG_DONTWAIT
	return socket_fd;
}


int serverListen(int port, ConcurrentQueue workQueue, void (*handler)(void*)){
	int status, new_connection_fd, socket_fd, rc, thread_ids = 0;

	socket_fd = getSocket(port);

	//listen on socket with a backlog of 10 connections
	status = listen(socket_fd, 100);
	if(status < 0){
		cout << "Error listening on bound socket" << endl;
		return status;
	} else{
		cout << "Listening on TCP socket" << endl;
	}

	//TODO: make this into an atomic flag
	while(true){
		//accept connections until program terminated
		char clientIp[INET_ADDRSTRLEN];
		cout << "Accepting new connections" << endl;

		struct sockaddr connection_addr;
		socklen_t connection_addr_size = sizeof(sockaddr);

		//block on accept until new connection
		new_connection_fd = accept(socket_fd, &connection_addr, &connection_addr_size);
		if(new_connection_fd < 0){
			cout << "Error accepting connection" <<endl;
		}

		//calculate ip of client
		int ip = ((struct sockaddr_in*)(&connection_addr))->sin_addr.s_addr;
		inet_ntop(AF_INET, &ip, clientIp, INET_ADDRSTRLEN);
		cout << "Accepted new connection from: " << string(clientIp) << " in socket: " << new_connection_fd << endl;

		//spin off new pthread to handle connection

		//setup thread and inputs
		pthread_t* current_connection_thread = (pthread_t*)malloc(sizeof(pthread_t));
		struct connectionArgs *args = new connectionArgs;
		args->thread_id = thread_ids;
		args->connection_fd = new_connection_fd;
		args->workQueue = workQueue;

		//spawn thread. we do not wait on these, since we do not care when they finish
		//these threads handle closing the socket themselves
		rc = pthread_create(current_connection_thread, NULL, handler, (void*)args);
		if(rc){
			cout << "Error creating thread: "<<thread_ids<<endl;
			return -1;
		}
		thread_ids++;
		threads.push_back(current_connection_thread);
	}
	return 0;
}

DFSServer::RequestOption getCommand(char* command){
	int stringLength = strlen(command);
	switch(stringLength){
		case 4:
			if(strncmp("LIST", command, 4) == 0){
				return DFSServer::RequestOption::LIST;
			}
			return DFSServer::RequestOption::NONE;
		case 3:
			if(strncmp("PUT", command, 3) == 0){
				return DFSServer::RequestOption::PUT;
			} else if(strncmp("GET", command, 3) == 0){
				return DFSServer::RequestOption::GET;
			}
			return DFSServer::RequestOption::NONE;
		default:
			return DFSServer::RequestOption::NONE;
	}
}

void handleConnection(void* args){
	int socket, bytesReceived, commandReceived = 0;
	char recv_buffer[BUFFER_SIZE+1];
	connectionArgs *inArgs = (connectionArgs)(args);
	socket = inArgs->connection_fd;
	ConcurrentQueue *queue = inArgs->workQueue;
	DFSServer::RequestOption command;
	string file = "";
	string fileContents = "";
	string user = "";
	string password = "";
	do{
		memset(recv_buffer, 0, BUFFER_SIZE+1);
		bytesReceived = recv(socket, recv_buffer, BUFFER_SIZE, NULL);
		if(bytesReceived <= 0){
			continue;
		}
		string currentData(recv_buffer);
		//parse the command and most of the file in first read
		if(!commandReceived){
			char tempBuffer[BUFFER_SIZE];
			strcpy(tempBuffer, currentData.c_str());
			char *command = strtok(tempBuffer, " ");
			command = getCommand(command);
			//format: <command> <user> <password> <file, if not LIST command> \n <file contents, if put>
			if(command != DFSServer::RequestOption::NONE){
				user = string(strtok(NULL, " "));
				password = string(strtok(NULL,  " "));
				//TODO: may need to support this in LIST as an optional parameter
				if(command != DFSServer::RequestOption::LIST){
					file = string(strtok(NULL, " "));
				}
				if(command == DFSServer::RequestOption::PUT){
					fileContents = string(currentData, currentData.find("\n"));
				}
			}
			commandReceived = 1;
		} else{
			//only should occur if is a large file and command is PUT
			if(command != DFSServer::RequestOption::PUT){
				break;
			}
			//add on any new content
			file.append(currentData);
		}
	} while(bytesReceived > 0);
	queue->push(command, user, password, file, fileContents, socket);
	pthread_exit(NULL);
}

void worker(void* args){
	workerArgs *inArgs = (workerArgs)(args);
	//TODO: for now, loop and wait if the queue is empty
	ConcurrentQueue *queue = inArgs->workQueue;
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;
	string user;
	string password;
	string file;
	string fileContents;
	DFSServer::RequestOption command;
	while(true){
		if(queue->getSize() <= 0){
			nanosleep(&sleepTime, &sleepTimeRem);
			continue;
		}
		QueueItem *request = queue->pop();
		command = request->command;
		user = request->user;
		password = request->password;
		file = request->file;
		fileContents = request->fileContents;
	}
	pthread_exit(NULL);
}


















