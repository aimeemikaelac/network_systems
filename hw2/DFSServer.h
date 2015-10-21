/*
 * DFSServer.h
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#ifndef DFSSERVER_H_
#define DFSSERVER_H_

class DFSServer {
public:
	DFSServer();
	virtual ~DFSServer();
	enum RequestOption{LIST, GET, PUT, NONE };
};

#endif /* DFSSERVER_H_ */
