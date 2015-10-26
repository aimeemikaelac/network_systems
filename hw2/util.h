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
#include <arpa/inet.h>
#include <map>
#include <list>
#include <vector>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <streambuf>
#include <set>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define BUFFER_SIZE 8192

enum RequestOption{ LIST,GET,PUT,MKDIR,NONE };

int getdir (std::string dir, std::string base, std::vector<std::string> &files);
bool isDirectory(std::string filePath);
int linuxCreateDirectoryTree(std::string path, int isFile);
int getFileSize(std::string file);
void checkFilepathExistsAccessible(std::string filePath, int* exists, int* accessible);
bool endsWith(char* str, char* suffix);
int sendErrorInvalidCredentials(int socket);
int sendErrorBadCommand(int socket);
int writeToSocket(int socket_fd, std::string line);
RequestOption getCommand(char* command);
int getSocket(int port);

#endif /* UTIL_H_ */
