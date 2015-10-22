/*
 * DFSServer.h
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#ifndef DFSSERVER_H_
#define DFSSERVER_H_

#include "util.h"

class DFSServer {
public:
	DFSServer(std::string dir, int port, std::string userTablePath);
	virtual ~DFSServer();
	enum RequestOption{LIST, GET, PUT, MKDIR, NONE };
	void runServer(std::string directory, int port);
	void parseUserTable(std::string table);
private:
	std::map<std::string, std::string> userTable;
};

#endif /* DFSSERVER_H_ */
