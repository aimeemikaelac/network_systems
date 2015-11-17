/*
 * webserver.h
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */

#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <list>
#include <wctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>

#include "webserver_util.h"
#include "ConcurrentQueue.h"

//use the maximum size of a request that is supported by most web servers
//theroetically
#define MAX_REQUEST_SIZE 8192

struct connectionArgs{
	int thread_id;
	int connection_fd;
	ConcurrentQueue *workQueue;
};

struct workerArgs{
	ConcurrentQueue *workQueue;
	double timeout;
};

typedef struct cacheEntry{
	std::string data;
	time_t timestamp;
} cacheEntry;

class Webserver{
	public:
		Webserver(int port, double timeout);
		~Webserver();
		int runServer();
	private:
		struct connectionArguments;
		double cacheTimeout;
		unsigned servicePort;
};

#endif
