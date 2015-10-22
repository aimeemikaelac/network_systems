/*
 * DFSClient.h
 *
 *  Created on: Oct 22, 2015
 *      Author: michael
 */

#ifndef DFSCLIENT_H_
#define DFSCLIENT_H_

#include "util.h"

class DFSClient {
public:
	DFSClient();
	virtual ~DFSClient();
	void cli();
	std::string parseCommand(std::string command);
private:
	void parseConfig(std::string file);
	std::string sendList();
	std::string sendGet(std::string filePath);
	std::string sendPut(std::string filePath);
	std::string sendMkdir(std::string filePath);
};

#endif /* DFSCLIENT_H_ */
