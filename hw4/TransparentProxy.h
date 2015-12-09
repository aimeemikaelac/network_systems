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
#include <linux/netfilter_ipv4.h>
#include <netinet/in.h>
#include <cstdio>
#include <memory>
#include <climits>
#include <stdint.h>

#define MAX_REQUEST_SIZE 8192

struct workerArgs{
	ConcurrentQueue *workQueue;
	std::string serverSide;
	int clientConnectionFd;
};

struct connectionArgs{
	int connection_fd;
	std::string clientSideIp;
	int clientSidePort;
};

class TransparentProxy {
public:
	TransparentProxy(std::string clientSide, std::string serverSide, int clientSidePort);
	virtual ~TransparentProxy();
	int runProxy();

private:
	int clientSidePort;
	std::string clientSide;
	std::string serverSide;
};

#endif /* TRANSPARENTPROXY_H_ */
