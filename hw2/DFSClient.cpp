/*
 * DFSClient.cpp
 *
 *  Created on: Oct 22, 2015
 *      Author: michael
 */

#include "DFSClient.h"

using namespace std;

DFSClient::DFSClient(string configFile) {
	parseConfig(configFile);
	cli();
}

DFSClient::~DFSClient() {
	// TODO Auto-generated destructor stub
}

void DFSClient::parseConfig(string file){
	int exists =0, accessible = 0;
	checkFilepathExistsAccessible(file, &exists, &accessible);
	//check if we can access the file
	if(exists == 0 || accessible == 0){
		cout << "Cannot access config file at: " << file <<endl;
		exit(-1);
	}
	ifstream fileStream(file.c_str());
	string line;
	int numServers = 0;
	while(!fileStream.eof()){
		if(getline(fileStream, line)){
			char tempLine[line.length()];
			strcpy(tempLine, line.c_str());
			char* firstElement = strtok(tempLine, " ");
			if(firstElement == NULL){
				continue;
			}
			if(strcmp("Server", firstElement) == 0){
				char* serverName = strtok(NULL, " ");
				if(serverName == NULL){
					continue;
				}
				char* ipAndPort = strtok(NULL, " ");
				if(ipAndPort == NULL){
					continue;
				}
				char* ip = strtok(ipAndPort, ":");
				if(ip == NULL){
					continue;
				}
				char* port = strtok(NULL, ":");
				if(port == NULL){
					continue;
				}
				serverToIpMap[string(serverName)] = string(ip);
				serverToPortMap[string(serverName)] = (int)strtol(port, NULL, 10);
				numServers++;
			} else if(strcmp("Username:", firstElement) == 0){
				char* username = strtok(NULL, " ");
				if(username == NULL){
					exit(-1);
				}
				user = string(user);
			} else if(strcmp("Password:", firstElement) == 0){
				char* pass = strtok(NULL, " ");
				if(pass == NULL){
					exit(-1);
				}
				password = string(pass);
			} else{
				continue;
			}
		}
	}
	if(numServers > 4){
		cout << "Too many servers for assignment" <<endl;
	}
	if(numServers < 4){
		cout << "Too few servers for assignment" <<endl;
	}
}

void DFSClient::cli(){
	string line;
	string response;
	while(true){
		cout << ">> ";
		getline(cin, line);
		if(cin.eof() == 1){
			return;
		}
		response =  parseCommand(line);
		cout << response << endl;
	}
}

struct fileNamePair DFSClient::getFilePart(string fileName){
	char temp[fileName.length()];
	string baseFile("");
	strcpy(temp, fileName.c_str());
	char *prev, *current;
	current = strtok(temp, ".");
	prev = "";
	while(current != NULL){
		baseFile.append(prev);
		prev = current;
		current = strtok(NULL, ".");
		if(current != NULL){
			baseFile.append(".");
		}
	}
	if(prev == NULL){
		return -1;
	}
	char *end;
	long partNum = strtol(prev, &end, 10);
	if(strcmp(end, "") != 0){
		return -1;
	}
	struct fileNamePair pair;
	pair.baseFile = baseFile;
	pair.fullFilePartName = fileName;
	return (int)partNum;
}

string DFSClient::writeStringToServer(string ip, int port, string message){
	int rc, socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server;
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		exit(-1);
	}
	if(inet_addr(ip.c_str()) == -1){
		cout << "Invalid ip address: "<<ip<<endl;
		exit(-1);
	}
	server.sin_addr.s_addr = inet_addr(ip.c_str());
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	rc = connect(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	if(rc < 0){
		cout << "Could not connect to server: "<<ip<<":"<<port<<endl;
		return "Failed to connect";
	}
	rc = send(socket_fd, message.c_str(), strlen(message.c_str()), 0);
	if(rc < 0){
		cout << "Could not send data to server: "<<ip<<":"<<port<<endl;
		return "Failed to send";
	}
	char buffer[BUFFER_SIZE+1];
	string response("");
	int bytesReceived;
	do{
		memset(buffer, 0, BUFFER_SIZE+1);
		bytesReceived = recv(socket_fd, buffer, BUFFER_SIZE, 0);
		if(bytesReceived < 0){
			cout << "Error receiving from socket"<<endl;
			return "Failed to receive";
		}
		response.append(buffer);
	} while(bytesReceived > 0);
	return response;
}

string DFSClient::writeListToServer(string ip, int port){
	string message = "LIST " + user + " " + password;
	string response = writeStringToServer(ip, port, message);
	return response;
}

string DFSClient::writeMkdirToServer(string ip, int port, string folder){
	string mkdirMessage = "MKDIR "+user+" "+password+" "+folder;
	return writeStringToServer(ip, port, mkdirMessage);
}

string DFSClient::writeGetToServer(string ip, int port, string file){
	string getMessage = "GET "+user+" "+password+" "+file;
	return writeStringToServer(ip, port, getMessage);
}

int DFSClient::parseListResponse(string server, string message){
	char temp[message.length()];
	strcpy(temp, message.c_str());
	char *totalFilesHeader = strtok(temp, " ");
	if(totalFilesHeader == NULL || (strcmp("Total_Files:", totalFilesHeader) != 0)){
		cout << "Could not parse total files header in LIST response" << endl;
		exit(-1);
	}
	char *totalFilesNumChar = strtok(NULL, " ");
	char *end;
	int totalFilesNum = strtol(totalFilesNumChar, &end, 10);
	if(strcmp(end, "") != 0){
		cout << "Extra characters after total files header in LIST request"<<endl;
		exit(-1);
	}
	if(totalFilesNum == 0){
		return 0;
	}
	char temp2[message.length()];
	strcpy(temp2, message.c_str());
	char *lineChar = strtok(temp2, "\n");
	string whitespaces (" \t\f\v\n\r");
	do{
		if(lineChar == NULL){
			break;
		}
		string line(lineChar);
		if(line.find("Total_Files:") != 0 && line.find_first_not_of(whitespaces) == 0 && line.find("Files:") != 0){
			struct fileNamePair pair = getFilePart(line);
			if(filesToParts.find(pair.baseFile) == filesToParts.end()){
				filesToParts[pair.baseFile] = new struct fileInfo;
			}
			vector<struct fileNamePair> currentInfo = filesToParts[pair.baseFile];
			pair.server = server;
			currentInfo.push_back(pair);
		}
		lineChar = strtok(NULL, " ");
	} while(lineChar != NULL);
	return totalFilesNum;
}

int DFSClient::numParts(string file){
	vector<struct fileNamePair> currentFileParts = filesToParts[file];
	vector<struct fileNamePair>::iterator fileIterator;
	int currentPart;
	int sum = 0;
	for(currentPart = 1; currentPart<=4; currentPart++){
		for(fileIterator = currentFileParts.begin(); fileIterator != currentFileParts.end(); fileIterator++){
			fileNamePair currentPair = *fileIterator;
			if(currentPair.part == currentPart){
				sum++;
				break;
			}
		}
	}
	return sum;
}

string DFSClient::sendList(){
	map<string, int>::iterator it;
	for(it = serverToIpMap.begin(); it != serverToIpMap.end(); it++){
		string server = (string)it->first;
		string ip = (string)it->second;
		int port = (int)serverToPortMap[server];
		string response = writeListToServer(ip, port);
		int files = parseListResponse(server, response);
	}
	string message = "LIST\n\n";
	map<string, vector<struct fileNamePair> >::iterator it2;
	for(it2 = filesToParts.begin(); it2 != filesToParts.end(); it2++){
		int parts= numParts(it2->first);
		message.append(it2->first);
		if(parts != 4){
			message.append(" [incomplete]");
		}
	}
	return message;
}

string DFSClient::sendGet(string filePath){
	//clear current info on files and rebuild, in case someone dropped out
	filesToParts.clear();
	sendList();
	if(numParts(filePath) < 4){
		return "Not enough parts to rebuild file";
	}
	vector<struct fileNamePair> currentFileInfo = filesToParts[filePath];
	int i;
	for(i=1; i<=4; i++){
		ofstream output;
		if(i==1){
			output.open(filePath.c_str(), ios_base::out);
		} else{
			output.open(filePath.c_str(), ios_base::app);
		}
		vector<string> servers;
		vector<struct fileNamePair>::iterator it;
		for(it = currentFileInfo.begin(); it != currentFileInfo.end(); it++){
			struct fileNamePair currentPair = *it;
			if(currentPair.part == i){
				servers.push_back(currentPair.server);
			}
		}
		vector<string>::iterator it2;
		string file;
		bool success = false;
		for(it2=servers.begin(); it2!=servers.end(); it2++){
			//TODO: will need to strip out the checksum values and verify at this stage
			string ip = serverToIpMap[*it];
			int port = serverToPortMap[*it];
			file = writeGetToServer(ip, port, filePath);
			if(file.find("Error") != 0){
				success = true;
				break;
			}
		}
		if(success){
			output << file;
		} else{
			return "Error receiving part: "+i;
		}
		output.flush();
		output.close();
	}
	return string("Did GET on: ").append(filePath);
}

string DFSClient::writePutToServer(string ip, int port, string fileName, string fileContents){
	string putMessage = "PUT " +user + " "+ password+" "+fileName+"\n"+fileContents;
	return writeStringToServer(ip, port, putMessage);
}

string DFSClient::sendPut(string filePath){
	int exists=0, accessible=0;
	checkFilepathExistsAccessible(filePath, &exists, &accessible);
	if(!(exists==1 && accessible== 1)){
		return "Could not access file to upload";
	}
	unsigned char digest[MD5_DIGEST_LENGTH];
	ifstream file(filePath.c_str());
	//read whole file contents into string
	string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
	int length = contents.length();
	string part1 = contents.substr(0, length/4);
	string part2 = contents.substr(length/4, length/2);
	string part3 = contents.substr(length/2, 3*(length/4));
	string part4 = contents.substr(3*(length/4));
	MD5((unsigned char*)contents.c_str(), strlen(contents.c_str()), digest);
	int partsToServer[4][2];
	memset(partsToServer, 0, 8*sizeof(int));
	int partsScheme = serverOrder[digest[MD5_DIGEST_LENGTH-1] % 4];
	int i,j;
	string message("");
	int successful;
	string response("");
	for(i=0; i<4; i++){
		for(j=0; j<2; j++){
			string server;
			string partToSend;
			switch(i){
				case 0:
					server = "DFS1";
					break;
				case 1:
					server = "DFS2";
					break;
				case 2:
					server = "DFS3";
					break;
				case 3:
					server = "DFS4";
					break;
			}
			int part =serverOrder[partsScheme][i][j];
			switch(part){
				case 1:
					partToSend = part1;
					break;
				case 2:
					partToSend = part2;
					break;
				case 3:
					partToSend = part3;
					break;
				case 4:
					partToSend = part4;
					break;
			}
			if(serverToIpMap.find(server) == serverToIpMap.end()){
				message.append("Could not find server "+server+" in configured servers\n");
			}
			string fileNameToSend = filePath + "."+part;
			string ip = serverToIpMap[server];
			int port = serverToPortMap[server];
			response.append("\n"+writePutToServer(ip, port, fileNameToSend, partToSend));
		}
	}
	return string("Did PUT on: "+filePath+"\nAnd received response: "+response);
}

string DFSClient::sendMkdir(string filePath){
	map<string, int>::iterator it;
	for(it = serverToIpMap.begin(); it != serverToIpMap.end(); it++){
		string server = (string)it->first;
		string ip = (string)it->second;
		int port = (int)serverToPortMap[server];
		string response = writeMkdirToServer(ip, port, filePath);
		if(strcmp("Created directory", response.c_str()) != 0){
			cout << "Error creating directory on server: "+server;
		}
	}
	return string("Did MKDIR on: ").append(filePath);
}

string DFSClient::parseCommand(string command){
	char temp[command.length()];
	strcpy(temp, command.c_str());
	char* inCommand = strtok(temp, " ");
	if(inCommand == NULL){
		return string("Invalid command: ").append(command);
	}
	RequestOption commandOption = getCommand(inCommand);
	if(commandOption == NONE){
		return string("Invalid command: ").append(command);
	}
	string response;
	char* filePath = strtok(NULL, " ");
	char* lastArg = filePath != NULL ? strtok(NULL, " ") : NULL;
	switch(commandOption){
		case LIST:
			response = filePath != NULL ? string("Invalid argument to LIST: ").append(filePath) : sendList();
			break;
		case GET:
			if(lastArg != NULL){
				response = string("GET does not take a third argument: ").append(lastArg);
			} else{
				response = filePath != NULL ? sendGet(string(filePath)) : string("GET requires a file path");
			}
			break;
		case PUT:
			if(lastArg != NULL){
				response = string("PUT does not take a third argument: ").append(lastArg);
			} else{
				response = filePath != NULL ? sendPut(string(filePath)) : string("PUT requires a file path");
			}
			break;
		case MKDIR:
			if(lastArg != NULL){
				response = string("MKDIR does not take a third argument: ").append(lastArg);
			} else{
				response = filePath != NULL ? sendMkdir(string(filePath)) : string("MKDIR requires a file path");
			}
			break;
		default:
			response = string("Invalid command: ").append(command);
	}
	return response;
}

int main(int argc, char **argv){
	DFSClient client("dfc.conf");
}











