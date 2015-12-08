/*
 * TransparentProxy.h
 *
 *  Created on: Dec 8, 2015
 *      Author: michael
 */

#ifndef TRANSPARENTPROXY_H_
#define TRANSPARENTPROXY_H_

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
#include <string>
#include <stdio.h>
#include <sys/stat.h>
#include "ConcurrentQueue.h"
#include <sys/types.h>

struct workerArgs{
	ConcurrentQueue *workQueue;
	std::string serverSide;
	int clientConnectionFd;
};

struct connectionArgs{
	int connection_fd;
	ConcurrentQueue *workQueue;
};

class TransparentProxy {
public:
	TransparentProxy();
	virtual ~TransparentProxy();

private:
	int runProxy(std::string clientSide, std::string serverSide, int clientSidePort);
};

#endif /* TRANSPARENTPROXY_H_ */
