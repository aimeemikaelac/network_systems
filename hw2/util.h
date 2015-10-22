/*
 * util.h
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <map>
#include <list>
#include <vector>
#include <dirent.h>
#include <errno.h>
#include "ConcurrentQueue.h"
#include "DFSServer.h"

#define BUFFER_SIZE 8192

typedef struct connectionArgs{
	int thread_id;
	int connection_fd;
	ConcurrentQueue *workQueue;
} connectionArgs;

typedef struct workerArgs{
	int thread_id;
	ConcurrentQueue *workQueue;
	std::map<std::string, std::list<std::string>> userTable;
} workerArgs;

#endif /* UTIL_H_ */
