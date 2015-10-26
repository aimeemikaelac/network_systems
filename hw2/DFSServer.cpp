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
}

/**
 * Print out the table of users and passwords
 */
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

/**
 * Run the DFS server by listening on a TCP port
 */
void DFSServer::runServer(string directory, int port){
	//if the directory does not exist, then we cannot continue
	if(chdir(directory.c_str()) < 0){
		cout << "Could not access the requested directory: "<<directory<<endl;
		return;
	}

	//create the work queue
	ConcurrentQueue queue;

	//setup inputs for the worker thread the processes requests off the work queue
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

/**
 * Parse the DFS config file, which is just a list of users and passwords
 */
void DFSServer::parseUserTable(string table){
	int exists = 0, accessible = 0;
	string line;
	char *user, *password;

	//check if the file exists
	checkFilepathExistsAccessible(table, &exists, &accessible);

	//if the file exists, then parse it
	if(exists && accessible){
		ifstream file(table.c_str());

		//loop through each line
		while(!file.eof()){
			if(getline(file, line)){
				//make copy of line
				char tempLine[line.length()];
				strcpy(tempLine, line.c_str());

				//get the username
				user = strtok(tempLine, " ");

				//get the password
				password = strtok(NULL, " ");

				//insert into the user table
				if(userTable.count(user) == 0){
					userTable[string(user)] = list<string>();
				}
				list<string> current = userTable[string(user)];
				current.push_back(string(password));
				userTable[string(user)] = current;
			}
		}
	}
}

/**
 * Listen on a TCP port for client connections
 */
int DFSServer::serverListen(int port, ConcurrentQueue workQueue){
	int status, new_connection_fd, socket_fd, rc, thread_ids = 0;

	//bind to a socket
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
	//loop forever accepting connections
	while(true){
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

/**
 * Task that a pthread executes to handle a client connection
 * Parses the request and puts onto the work queue if it is valid
 */
static void* handleConnection(void* args){
	int socket, bytesReceived, commandReceived = 0;
	char recv_buffer[BUFFER_SIZE+1];
	string file = "", fileContents = "", user = "", password = "";
	connectionArgs *inArgs = (connectionArgs*)(args);
	socket = inArgs->connection_fd;
	ConcurrentQueue *queue = inArgs->workQueue;
	RequestOption command;
	int length = 0, readFileBytes = 0;
	bool continueToReceive = true;
	char *commandChar;

	//Read data off the socket and parse
	do{
		memset(recv_buffer, 0, BUFFER_SIZE+1);
		bytesReceived = recv(socket, recv_buffer, BUFFER_SIZE, 0);
		if(bytesReceived <= 0){
			continue;
		}
		string currentData(recv_buffer);

		//parse the command and most of the file in first read
		//TODO: need to treat file data as raw bytes instead of as an ascii string
		if(!commandReceived){
			char tempBuffer[BUFFER_SIZE];
			strcpy(tempBuffer, currentData.c_str());
			commandChar = strtok(tempBuffer, " ");

			//lookup command
			command = getCommand(commandChar);

			//parse command
			//format: <command> <user> <password> <file, if not LIST command> \n <file contents, if put>
			if(command != NONE){
				user = string(strtok(NULL, " "));
				password = string(strtok(NULL,  " "));

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

/**
 * Task that the worker thread executes the process the work queue
 * Reads a request off of the queue and performs the command
 */
static void* workerThread(void* args){
	workerArgs *inArgs = (workerArgs*)(args);
	ConcurrentQueue *queue = inArgs->workQueue;
	struct timespec sleepTimeRem;
	string user, password, file, fileContents;
	list<string> passwordList;
	int socket, validPassword;
	RequestOption command;
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	map<string, list<string> > localUserTable = *(inArgs->userTable);
	QueueItem *request;

	//loop and continue processing work off queue
	//TODO: for now, loop and wait if the queue is empty
	while(true){
		//sleep
		if(queue->getSize() <= 0){
			nanosleep(&sleepTime, &sleepTimeRem);
			continue;
		}

		//get next item, blocking till available
		request = queue->pop();

		//parse out connection and command
		socket = request->socket;
		command = request->command;

		//if not a valif command, send error
		if(command == NONE){
			sendErrorBadCommand(socket);
		}

		//parse user
		user = request->user;

		//if user does not exist, send error
		if(localUserTable.count(user) == 0){
			sendErrorInvalidCredentials(socket);
		}

		//parse password
		password = request->password;
		passwordList = localUserTable[user];
		validPassword = 0;

		//lookup password
		for(std::list<string>::iterator it = passwordList.begin(); it != passwordList.end(); it++){
			string current = (string)(*it);
			if(current == password){
				validPassword = 1;
				break;
			}
		}

		//if password cannot be found for user, send error
		if(validPassword == 0){
			cout << "Could not find password"<<endl;
			sendErrorInvalidCredentials(socket);
		}

		//parse file. may not be used by all commands
		file = request->file;
		fileContents = request->fileContents;

		//process command based on parsed info
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

		//finally, close socket when done
		close(socket);
	}
	pthread_exit(NULL);
}

/**
 * Perform a LIST command. Looks up files for the current user/password pair
 */
static void doList(int socket, string user, string password){
	int i, exists = 0, accessible = 0;
	string message = "Total_Files: ";
	vector<string> files;
	char* currentwd = getcwd(NULL, 0);

	//construct path to user's directory
	string userDir(currentwd);
	userDir = userDir.append("/");
	userDir = userDir.append(user);
	userDir = userDir.append("/");
	userDir = userDir.append(password);

	//make sure it exists
	checkFilepathExistsAccessible(userDir, &exists, &accessible);

	//if it exists, get all of the files in the directory
	if(exists && accessible){
		getdir(userDir, "", files);
	}

	//convert the number of files to a string
	char num[100];
	sprintf(num, "%d", (int)files.size());

	//add value to header
	message.append(num);

	//add next header
	message.append("\nFiles:");

	//add each file to the message
	for(i=0; (unsigned)i<files.size(); i++){
		message.append("\n");
		message.append(files[i]);
	}

	//write message to client
	writeToSocket(socket, message);
	free(currentwd);
}

/**
 * Perform a MKDIR command. Creates a directory in a user's directory
 */
static void doMkdir(int socket, string user, string password, string file){
	string dir(user);
	dir.append("/");
	dir.append(password);
	dir.append("/");
	dir.append(file);

	//recursively create the directory tree
	if(linuxCreateDirectoryTree(dir, 0) < 0){
		writeToSocket(socket, "Error: Could not create directory");
	} else{
		writeToSocket(socket, "Created directory");
	}
}

/**
 * Perform a GET command. Reads a file from a user's directory and sends it
 * to the client
 */
static void doGet(int socket, string user, string password, string file){
	int exists = 0, accessible = 0;
	char *currentwd = getcwd(NULL, 0);
	string path(currentwd);
	path.append("/");
	path.append(user);
	path.append("/");
	path.append(password);
	path.append("/");
	path.append(file);

	//check if file exists
	checkFilepathExistsAccessible(path, &exists, &accessible);

	//if it exists and is not a directory, then write
	//the file to the socket line-by-line
	if(exists && accessible && !isDirectory(path)){
		ifstream file(path.c_str());
		string line;
		//TODO: skip some lines depending on the hash storage
		//or it may be handled by the client
		while(!file.eof()){
			//TODO: strip out last unneeded newline so the client doesn't have to
			//as it is not part of the original file
			if(getline(file, line)){
				writeToSocket(socket, line);
			}
		}
	}else{
		writeToSocket(socket, "Error: Could not access file");
	}
	free(currentwd);
}

/**
 * Perform a PUT command. Receives a file from the client and writes it to the user;s directory
 */
static void doPut(int socket, string user, string password, string file, string fileContents){
	int exists = 0, accessible = 0;
	char *currentwd = getcwd(NULL, 0);
	string path("");
	path.append(user);
	path.append("/");
	path.append(password);
	path.append("/");
	path.append(file);

	//recursively create the directory tree to where the file needs to go
	linuxCreateDirectoryTree(path, 1);

	//check if the file exists
	checkFilepathExistsAccessible(path, &exists, &accessible);

	//if it exists and we cannot access it, give up
	//also give up if it is a directory
	if(exists && !accessible){
		return;
	} else if(isDirectory(path)){
		return;
	} else{
		//TODO: send a return message
		//write the entire file contents to the file
		cout << "Writing file"<<endl;
		ofstream file;
		file.open(path.c_str(), ofstream::out|ofstream::trunc);
		file << fileContents;
	}
	free(currentwd);
}

int main(int argc, char** argv){
	string userTableFile = "./dfs.conf";
	if(argc<3){
		cout << "Incorrect number of arguments" <<endl;
		return -1;
	}
	string directory(argv[1]);
	int port = atoi(argv[2]);
	DFSServer server(directory, port, userTableFile);
}
