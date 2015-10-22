/*
 * DFSClient.cpp
 *
 *  Created on: Oct 22, 2015
 *      Author: michael
 */

#include "DFSClient.h"

using namespace std;

DFSClient::DFSClient() {
	// TODO Auto-generated constructor stub

}

DFSClient::~DFSClient() {
	// TODO Auto-generated destructor stub
}

void DFSClient::cli(){
	string line;
	string response;
	while(true){
		cout << ">>";
		getline(cin, line);
		response =  parseCommand(line);
		cout << response << endl;
	}
}

string DFSClient::sendList(){
	return "Did List";
}

string DFSClient::sendGet(string filePath){
	return string("Did GET on: ").append(filePath);
}

string DFSClient::sendPut(string filePath){
	return string("Did PUT on: ").append(filePath);
}

string DFSClient::sendMkdir(string filePath){
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













