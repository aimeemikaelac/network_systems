/*
 * DFSServer.cpp
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#include "DFSServer.h"

using namespace std;

DFSServer::DFSServer(string dir, int port, string userTablePath) {
	string actualTable = dir;
	actualTable.append("/");
	actualTable.append(userTablePath);
	parseUserTable(actualTable);
	runServer(dir, port);
}

DFSServer::~DFSServer() {
	// TODO Auto-generated destructor stub
}

void printUserTable(map<string, list<string> > &table){
	map<string, list<string> >::iterator mapIt;
	for(mapIt= table.begin(); mapIt!=table.end(); mapIt++){
		cout << "Username: "<<mapIt->first<<" Passwords:";
		list<string>::iterator listIt;
		for(listIt = mapIt->second.begin(); listIt!= mapIt->second.end(); listIt++){
			cout << *listIt+" ";
		}
		cout << endl;
	}
}

void DFSServer::runServer(string directory, int port){
	if(chdir(directory.c_str()) < 0){
		cout << "Could not access the requested directory: "<<directory<<endl;
		return;
	}
	ConcurrentQueue queue;
	struct workerArgs *args = new workerArgs;
	args->workQueue = &queue;
	args->userTable = &userTable;
	printUserTable(userTable);
	//spawn worker thread
	pthread_t* worker_thread = (pthread_t*)malloc(sizeof(pthread_t));
	int rc = pthread_create(worker_thread, NULL, workerThread, (void*)args);
	if(rc){
		cout << "Error creating worker thread " << endl;
		return;
	}
	//now listen on the port forever
	serverListen(port, queue);
}

void DFSServer::parseUserTable(string table){
	int exists = 0, accessible = 0;
	checkFilepathExistsAccessible(table, &exists, &accessible);
	if(exists && accessible){
		ifstream file(table.c_str());
		string line;
		while(!file.eof()){
			if(getline(file, line)){
				char tempLine[line.length()];
				strcpy(tempLine, line.c_str());
				char *user = strtok(tempLine, " ");
				char *password = strtok(NULL, " ");
				if(userTable.count(user) == 0){
					cout << "Parsed user: "<<user<<" Password: "<<password<<endl;
					userTable[string(user)] = list<string>();
				}
				list<string> current = userTable[string(user)];
				current.push_back(string(password));
				userTable[string(user)] = current;
				cout << "Current password list: "<<current.size()<<endl;
			}
		}
	}
}

int DFSServer::serverListen(int port, ConcurrentQueue workQueue){
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
		args->workQueue = &workQueue;

		//spawn thread. we do not wait on these, since we do not care when they finish
		//these threads handle closing the socket themselves
		rc = pthread_create(current_connection_thread, NULL, handleConnection, (void*)args);
		if(rc){
			cout << "Error creating thread: "<<thread_ids<<endl;
			return -1;
		}
	}
	return 0;
}

static void* handleConnection(void* args){
	int socket, bytesReceived, commandReceived = 0;
	char recv_buffer[BUFFER_SIZE+1];
	connectionArgs *inArgs = (connectionArgs*)(args);
	socket = inArgs->connection_fd;
	ConcurrentQueue *queue = inArgs->workQueue;
	RequestOption command;
	string file = "";
	string fileContents = "";
	string user = "";
	string password = "";
	int length = 0;
	int readFileBytes = 0;
	bool continueToReceive = true;
	do{
		memset(recv_buffer, 0, BUFFER_SIZE+1);
		bytesReceived = recv(socket, recv_buffer, BUFFER_SIZE, 0);
		if(bytesReceived <= 0){
			continue;
		}
		string currentData(recv_buffer);
		//parse the command and most of the file in first read
		if(!commandReceived){
			char tempBuffer[BUFFER_SIZE];
			cout << "Received message: "<<endl<<currentData<<endl;
			strcpy(tempBuffer, currentData.c_str());
			char *commandChar = strtok(tempBuffer, " ");
			command = getCommand(commandChar);
			cout << "Parsed command: "<<commandChar<<endl;
			//format: <command> <user> <password> <file, if not LIST command> \n <file contents, if put>
			if(command != NONE){
				cout << "Parsing user"<<endl;
				user = string(strtok(NULL, " "));
				cout << "Parsed user: "<<user<<endl;
				cout << "Parsing password"<<endl;
				password = string(strtok(NULL,  " "));
				cout << "Parsed password: "<<password<<endl;
				//TODO: may need to support this in LIST as an optional parameter
				if(command != LIST){
					file = string(strtok(NULL, " "));
				}
				if(command == PUT){
					char* lengthChar = strtok(NULL, " ");
					length = atoi(lengthChar);
					fileContents = string(currentData, currentData.find("\n")+1);
					readFileBytes += fileContents.size();
				}
			}
			commandReceived = 1;
			if(command != PUT){
				continueToReceive = false;
			} else if(readFileBytes >= length){
				continueToReceive = false;
			}
		} else{
			//only should occur if is a large file and command is PUT
			if(command != PUT){
				break;
			}
			//add on any new content
			file.append(currentData);
			readFileBytes+=currentData.size();
			if(readFileBytes >= length){
				break;
			}
		}
	} while(bytesReceived > 0 && continueToReceive);
	queue->push(command, user, password, file, fileContents, length, socket);
	pthread_exit(NULL);
}

static void* workerThread(void* args){
	workerArgs *inArgs = (workerArgs*)(args);
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
	list<string> passwordList;
	int socket;
	RequestOption command;
	map<string, list<string> > localUserTable = *(inArgs->userTable);
	printUserTable(localUserTable);
	while(true){
		if(queue->getSize() <= 0){
			nanosleep(&sleepTime, &sleepTimeRem);
			continue;
		}
		QueueItem *request = queue->pop();
		socket = request->socket;
		command = request->command;
		if(command == NONE){
			sendErrorBadCommand(socket);
		}
		user = request->user;
		if(localUserTable.count(user) == 0){
			cout << "Could not find username"<<endl;
			sendErrorInvalidCredentials(socket);
		}
		password = request->password;
		passwordList = localUserTable[user];
		int validPassword = 0;
		cout << "Number of passwords: "<<passwordList.size()<<endl;
		for(std::list<string>::iterator it = passwordList.begin(); it != passwordList.end(); it++){
			string current = (string)(*it);
			cout << "Current password: "<<current<<endl;
			if(current == password){
				validPassword = 1;
				break;
			}
		}
		if(validPassword == 0){
			cout << "Could not find password"<<endl;
			sendErrorInvalidCredentials(socket);
		}
		file = request->file;
		fileContents = request->fileContents;
		cout << "Worker processing request"<<endl;
		switch(command){
			case LIST:
				doList(socket, user, password);
				break;
			case MKDIR:
				doMkdir(socket, user, password, file);
				break;
			case GET:
				doGet(socket, user, password, file);
				break;
			case PUT:
				//TODO: if the file is too large to store in memory, put into a
				//temp file in the handler and pass the path to the temp file instead
				doPut(socket, user, password, file, fileContents);
				break;
			case NONE: default:
				sendErrorBadCommand(socket);
				break;
		}
		close(socket);
	}
	pthread_exit(NULL);
}

//void printHelp(){
//	cout << "Usage: dfs [-f FILE] [-p PORT] [-h]" << endl;
//	cout << "Options:" << endl;
//	cout << "  -f FILE\tSpecify a configuration file. If the file cannot be opened, reasonable defaults will be assumed. See README for format. The default is ./dfs.conf."<<endl;
//	cout << "  -p PORT\tSpecify a service port. Will be ignored if port is set in a configuration file. The default is 8000." <<endl;
//	cout << "  -h\t\tPrint this message and exit."<<endl;
//}

int main(int argc, char** argv){
//	int opt;
	string userTableFile = "./dfs.conf";
	if(argc<3){
		cout << "Incorrect number of arguments" <<endl;
		return -1;
	}
	string directory(argv[1]);
	int port = atoi(argv[2]);
	DFSServer server(directory, port, userTableFile);
//	bool regularDirectory = false
//	if(argc > 1){
//		while(optind < argc){
//			if((opt = getopt(argc, argv, "f:d:p:h")) != -1){
//				switch(opt){
//					case 'f':
//						configFile = string(optarg);
//						break;
//					case 'p':
//						port = atoi(optarg);
//						break;
//					case 'h':
//						printHelp();
//						return -1;
//					case '?':
//						printHelp();
//						return -1;
//				}
//			} else{
//
//			}
//		}
//	}
}
