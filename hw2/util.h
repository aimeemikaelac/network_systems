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

#define BUFFER_SIZE 8192

enum RequestOption{ LIST,GET,PUT,MKDIR,NONE };

void doPut(int socket, std::string user, std::string password, std::string file, std::string fileContents);
void doGet(int socket, std::string user, std::string password, std::string file);
void doMkdir(int socket, std::string user, std::string password, std::string file);
void doList(int socket, std::string user, std::string password);
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
