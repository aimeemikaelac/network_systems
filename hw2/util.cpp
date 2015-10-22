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
		case 5:
			if(strncmp("MKDIR", command, 5) == 0){
				return DFSServer::RequestOption::MKDIR;
			}
			return DFSServer::RequestOption::NONE;
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

int writeToSocket(int socket_fd, string line){
	int written = write(socket_fd, line.c_str(), line.length());
	written += write(socket_fd, "\n", strlen("\n"));
	return written;
}

int sendErrorBadCommand(int socket){
	return writeToSocket(socket, "Invalid command");
}

int sendErrorInvalidCredentials(int socket){
	return writeToSocket(socket, "Invalid Username/Password. Please try again.");
}


//1 if str ends with suffix, 0 otherwise
bool endsWith(char* str, char* suffix){
	if(!str || !suffix){
		return 0;
	}
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix >  lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void checkFilepathExistsAccessible(string filePath, int* exists, int* accessible){
	if(access(filePath.c_str(), F_OK) == 0){
		*exists = 1;
	} else{
		*exists = 0;
		*accessible = 0;
		return;
	}
	if(access(filePath.c_str(), R_OK) == 0){
			*accessible = 1;
	} else{
		*accessible = 0;
		return;
	}
}

int getFileSize(string file){
//	struct stat stat_buf;
//	int rc = stat(file.c_str(), &stat_buf);
//	return rc == 0? stat_buf.st_size : -1;
	FILE *p_file = NULL;
	p_file = fopen(file.c_str(),"rb");
	fseek(p_file,0,SEEK_END);
	int size = ftell(p_file);
	fclose(p_file);
	return size;
}

int linuxCreateDirectoryTree(string path, int isFile){
	int i, exists = 0, accessible = 0;
	char* currentwd = getcwd(NULL, 0);
	checkFilepathExistsAccessible(path, &exists, &accessible);
	if(exists == 1){
		if(accessible == 0){
			return -1;
		} else{
			return 0;
		}
	}
	vector<string> directories;
	char temp[path.npos];
	strcpy(temp, path.c_str());
	char *current = strtok(temp, "/");
	while(current != NULL){
		directories.push_back(string(current));
	}
	string currentDir(currentwd);
	int startingSize = directories.size();
	for(i=0; i<startingSize; i++){
		if(isFile && i == directories.size() - 1){
			break;
		}
		currentDir.append("/");
		currentDir.append(directories[i]);
		exists = 0;
		accessible = 0;
		checkFilepathExistsAccessible(currentDir, &exists, &accessible);
		if(exists && accessible){
			continue;
		} else if(exists == 0){
			mkdir(currentDir.c_str(), 0700);
		} else if(accessible == 0){
			return -1;
		}
	}
	free(currentwd);
}

bool isDirectory(string filePath){
	struct stat s;
	if(stat(filePath.c_str(), &s) == 0){
		if(s.st_mode & S_IFDIR){
			return true;
		}
	}
	return false;
}

//get files in a directory recursively, for all subdirs
int getdir (string dir, string base, vector<string> &files)
{
    DIR *dp;
    struct dirent *dirp;
//    struct stat stat_buf;
    vector<string> directories;
    vector<string> baseDirs;
    if((dp  = opendir(dir.c_str())) == NULL) {
//        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
    	string file = dir;
    	file = file.append("/");
    	file = file.append(dirp->d_name);
    	string fileStore = base;
    	fileStore = fileStore.append("/");
    	fileStore = fileStore.append(dirp->d_name);
//    	stat(file.c_str(), &stat_buf);
//		if(S_ISDIR(stat_buf.st_mode)){
    	if(isDirectory(file)){
			directories.push_back(file);
			baseDirs.push_back(string(dirp->d_name));
		} else{
			files.push_back(fileStore);
		}

    }
    closedir(dp);
    int i;
    for(i=0; i<directories.size(); i++){
    	getdir(directories[i], baseDirs[i], files);
    }
    return 0;
}

void doList(int socket, string user, string password){
	int i, exists = 0, accessible = 0, numFiles = 0;
//	list<string> files;
	string message = "Total files: ";
	char* currentwd = getcwd(NULL, 0);
	string userDir(currentwd);
	userDir = userDir.append("/");
	userDir = userDir.append(user);
	userDir = userDir.append("/");
	userDir = userDir.append(password);
	checkFilepathExistsAccessible(userDir, &exists, &accessible);
	vector<string> files;
	if(exists && accessible){
		getdir(userDir, "", files);
	}
	message.append(string(files.size()));
	message.append("\nFiles:");
	for(i=0; i<files.size(); i++){
		message.append("\n");
		message.append(files[i]);
	}
	writeToSocket(socket, message);
	free(currentwd);
}

void doMkdir(int socket, string user, string password, string file){
	string dir(user);
	dir.append("/");
	dir.append(password);
	dir.append("/");
	dir.append(file);
	if(linuxCreateDirectoryTree(dir, 0) < 0){
		writeToSocket(socket, "Could not create directory");
	} else{
		writeToSocket(socket, "Created directory");
	}
}

void doGet(int socket, string user, string password, string file){
	int exists = 0, accessible = 0;
	char *currentwd = getcwd(NULL, 0);
	string path(currentwd);
	path.append("/");
	path.append(user);
	path.append("/");
	path.append(password);
	path.append("/");
	path.append(file);
	checkFilepathExistsAccessible(path, &exists, &accessible);
	if(exists && accessible && !isDirectory(path)){
		ifstream file(path.c_str());
		string line;
		//TODO: skip some lines depending on the hash storage
		//or it may be handled by the client
		while(!file.eof()){
			if(getline(file, line)){
				writeToSocket(socket, line);
			}
		}
	}else{
		writeToSocket(socket, "Could not access file");
	}
	free(currentwd);
}

void doPut(int socket, string user, string password, string file, string fileContents){
	int exists = 0, accessible = 0;
	char *currentwd = getcwd(NULL, 0);
	string path(currentwd);
	path.append("/");
	path.append(user);
	path.append("/");
	path.append(password);
	path.append("/");
	path.append(file);
	checkFilepathExistsAccessible(path, &exists, &accessible);
	if(exists || !accessible || isDirectory(path)){
		return;
	} else{
		ofstream file(path);
		file << fileContents;
	}
	free(currentwd);
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
	list<string> passwordList;
	int socket;
	DFSServer::RequestOption command;
	map<string, string> userTable = inArgs->userTable;
	while(true){
		if(queue->getSize() <= 0){
			nanosleep(&sleepTime, &sleepTimeRem);
			continue;
		}
		QueueItem *request = queue->pop();
		socket = request->socket;
		command = request->command;
		if(command == DFSServer::RequestOption::NONE){
			sendErrorBadCommand(socket);
		}
		user = request->user;
		if(userTable.find(user) == userTable.end()){
			sendErrorInvalidCredentials(socket);
		}
		password = request->password;
		passwordList = userTable[user];
		int validPassword = 0;
		for(std::list<int>::iterator it = passwordList.begin(); it != passwordList.end(); it++){
			string current = (string)(*it);
			if(current == password){
				validPassword = 1;
				break;
			}
		}
		if(validPassword == 0){
			sendErrorInvalidCredentials(socket);
		}
		file = request->file;
		fileContents = request->fileContents;
		switch(command){
			case DFSServer::RequestOption::LIST:
				doList(socket, user, password);
				break;
			case DFSServer::RequestOption::MKDIR:
				doMkdir(socket, user, password, file);
				break;
			case DFSServer::RequestOption::GET:
				doGet(socket, user, password, file);
				break;
			case DFSServer::RequestOption::PUT:
				//TODO: if the file is too large to store in memory, put into a
				//temp file in the handler and pass the path to the temp file instead
				doPut(socket, user, password, file, fileContents);
				break;
			case DFSServer::RequestOption::NONE: Default:
				sendErrorBadCommand(socket);
				break;
		}
	}
	pthread_exit(NULL);
}


















