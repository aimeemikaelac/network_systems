/*
 * DFSClient.h
 *
 *  Created on: Oct 22, 2015
 *      Author: michael
 */

#ifndef DFSCLIENT_H_
#define DFSCLIENT_H_

#include "util.h"

struct fileNamePair{
	std::string baseFile;
	std::string fullFilePartName;
	std::string server;
	int part;
};

int serverOrder[4][4][2] = {
	{
		{1,2}, {2,3}, {3,4}, {4,1}
	},
	{
		{4,1}, {1,2}, {2,3}, {3,4}
	},
	{
		{3,4}, {4,1}, {1,2}, {2,3}
	},
	{
		{2,3}, {3,4}, {4,1}, {1,2}
	}
};

class DFSClient {
public:
	DFSClient(std::string configFile);
	virtual ~DFSClient();
	void cli();
	std::string parseCommand(std::string command);
private:
	void parseConfig(std::string file);
	std::string sendList();
	std::string sendGet(std::string filePath);
	std::string sendPut(std::string filePath);
	std::string sendMkdir(std::string filePath);
	std::map<std::string, std::string> serverToIpMap;
	std::map<std::string, int> serverToPortMap;
	std::string password;
	std::string user;
	std::map<std::string, std::vector<struct fileNamePair> > filesToParts;
	std::string writeListToServer( std::string ip, int port);
	std::string writeMkdirToServer( std::string ip, int port, std::string folder);
	std::string writeGetToServer( std::string ip, int port, std::string file);
	std::string writePutToServer( std::string ip, int port, std::string fileName, std::string fileContents);
	std::string writeStringToServer(std::string ip, int port, std::string message);
	int numParts(std::string file);
	int parseListResponse(std::string server, std::string message);
	struct fileNamePair DFSClient::getFilePart(std::string fileName);
};

#endif /* DFSCLIENT_H_ */
