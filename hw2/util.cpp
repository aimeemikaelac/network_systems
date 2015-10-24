/*
 * util.c
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#include "util.h"

using namespace std;

int getSocket(int port){
	int socket_fd, status;
	struct sockaddr_in server_addr;
	char service[100];
	sprintf(service, "%i", port);

	//setup struct to create socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

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
		cout << "Running HTTP server on TCP port: "<<port<<endl;
	}
	return socket_fd;
}




RequestOption getCommand(char* command){
	int stringLength = strlen(command);
	switch(stringLength){
		case 5:
			if(strncmp("MKDIR", command, 5) == 0){
				return MKDIR;
			}
			return NONE;
		case 4:
			if(strncmp("LIST", command, 4) == 0){
				return LIST;
			}
			return NONE;
		case 3:
			if(strncmp("PUT", command, 3) == 0){
				return PUT;
			} else if(strncmp("GET", command, 3) == 0){
				return GET;
			}
			return NONE;
		default:
			return NONE;
	}
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
	char temp[path.length()];
	strcpy(temp, path.c_str());
	char *current = strtok(temp, "/");
	while(current != NULL){
		directories.push_back(string(current));
	}
	string currentDir(currentwd);
	int startingSize = directories.size();
	for(i=0; i<startingSize; i++){
		if(isFile && (unsigned)i == directories.size() - 1){
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
	return 0;
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
    for(i=0; (unsigned)i<directories.size(); i++){
    	getdir(directories[i], baseDirs[i], files);
    }
    return 0;
}

void doList(int socket, string user, string password){
	int i, exists = 0, accessible = 0;
//	list<string> files;
	string message = "Total_Files: ";
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
	char num[100];
	sprintf(num, "%d", (int)files.size());
	message.append(num);
	message.append("\nFiles:");
	for(i=0; (unsigned)i<files.size(); i++){
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
		writeToSocket(socket, "Error: Could not create directory");
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
		writeToSocket(socket, "Error: Could not access file");
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
	//TODO: send a return message
	if(exists || !accessible || isDirectory(path)){
		return;
	} else{
		ofstream file(path.c_str());
		file << fileContents;
	}
	free(currentwd);
}




















