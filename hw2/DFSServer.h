/*
 * DFSServer.h
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#ifndef DFSSERVER_H_
#define DFSSERVER_H_

#include "util.h"
#include "ConcurrentQueue.h"

typedef struct connectionArgs{
	int thread_id;
	int connection_fd;
	ConcurrentQueue *workQueue;
} connectionArgs;

typedef struct workerArgs{
	int thread_id;
	ConcurrentQueue *workQueue;
	std::map<std::string, std::list<std::string> > *userTable;
} workerArgs;

static void* workerThread(void* args);
static void* handleConnection(void* args);
static void doPut(int socket, std::string user, std::string password, std::string file, std::string fileContents);
static void doGet(int socket, std::string user, std::string password, std::string file);
static void doMkdir(int socket, std::string user, std::string password, std::string file);
static void doList(int socket, std::string user, std::string password);

class DFSServer {
public:
	DFSServer(std::string dir, int port, std::string userTablePath);
	virtual ~DFSServer();
	void runServer(std::string directory, int port);
	void parseUserTable(std::string table);
private:
	std::map<std::string, std::list<std::string> > userTable;
	int serverListen(int port, ConcurrentQueue workQueue);
};

#endif /* DFSSERVER_H_ */
