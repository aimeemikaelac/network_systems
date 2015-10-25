/*
 * DFSClient.h
 *
 *  Created on: Oct 22, 2015
 *      Author: michael
 */

#ifndef DFSCLIENT_H_
#define DFSCLIENT_H_

#include "util.h"

typedef struct fileNamePair{
	std::string baseFile;
	std::string fullFilePartName;
	std::string server;
	int part;
} fileNamePair;

typedef struct encryptedFile{
	unsigned char* contents;
	int contentLength;
	unsigned char *iv;
} encryptedFile;

typedef struct receivedResponse{
	unsigned char* contents;
	int length;
} receivedResponse;

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

#define SERVER_PT_PASSWORD 1
#define HASH_ROUNDS 1000

class DFSClient {
public:
	DFSClient(std::string configFile);
	virtual ~DFSClient();
	void cli();
	std::string parseCommand(std::string command);
private:
	unsigned char* serverHashedPassword;
	bool serverHashPerformed;
	void parseConfig(std::string file);
	std::string sendList();
	std::string sendGet(std::string filePath);
	std::string sendPut(std::string filePath);
	std::string sendMkdir(std::string filePath);
	std::map<std::string, std::string> serverToIpMap;
	std::map<std::string, int> serverToPortMap;
	std::string password;
	std::string user;
	std::map<std::string, std::vector<fileNamePair> > filesToParts;
	std::string writeListToServer( std::string ip, int port);
	std::string writeMkdirToServer( std::string ip, int port, std::string folder);
	receivedResponse writeGetToServer( std::string ip, int port, std::string file);
	std::string writePutToServer( std::string ip, int port, std::string fileName, unsigned char* fileContents, int contentsLength);
	receivedResponse writeStringToServer(std::string ip, int port, unsigned char* message, int messageLength);
	int numParts(std::string file);
	int parseListResponse(std::string server, std::string message);
	fileNamePair getFilePart(std::string fileName);
	encryptedFile* readAndEncrypt(std::string filePath);
	std::string getServerPasswordHashString(unsigned char* hash);
	std::string getServerPasswordHashStringDirect();
	void decryptAndWrite(std::string filePath, encryptedFile encryptedContents);
	void getSha256String(unsigned char *input, int inputLength, unsigned char *output);
	void getSha256Key(unsigned char *output);
	void getRandomIv(unsigned char* iv);
	void getServerPasswordHash(unsigned char* out, int outLength);
	std::string md5ToHexString(unsigned char* hash);
	std::string ivToHexString(unsigned char* iv);
	void hexToIvString(std::string ivHex, unsigned char *iv);
};

#endif /* DFSCLIENT_H_ */
