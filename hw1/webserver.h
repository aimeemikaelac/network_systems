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
	std::string documentRoot;
	std::set<std::string> documentIndex;
	std::map<std::string, std::string> contentTypes;
};

struct workerArgs{
	int socket_fd;
	std::string documentRoot;
	std::set<std::string> documentIndex;
	std::map<std::string, std::string> contentTypes;
	ConcurrentQueue *workQueue;
	volatile bool continue_processing;
	volatile bool flush_queue;
	volatile bool workerEndConnection;
	pthread_mutex_t *lock;
	pthread_mutex_t *worker_end_lock;
};

class Webserver{
	public:
		Webserver(std::string, int port);
		~Webserver();
		int runServer();
		enum ConfigOptions{ServicePort, DocumentRoot, DirectoryIndex, ContentType };
	private:
		int parseFile(std::string);
		bool parseOption(std::string, ConfigOptions);
		struct connectionArguments;
		enum ConfigOptions;
		std::string fileName;
		std::string documentRoot;
		std::set<std::string> documentIndex;
		std::map<std::string, std::string> contentTypes;
		unsigned servicePort;
};

#endif
