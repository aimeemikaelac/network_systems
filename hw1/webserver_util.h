/*
 * webserver_util.h
 *
 *  Created on: Sep 18, 2015
 *      Author: michael
 */

#ifndef WEBSERVER_UTIL_H_
#define WEBSERVER_UTIL_H_

#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>

//1 if str ends with suffix, 0 otherwise
bool endsWith(char* str, char* suffix);

void checkFilepathExistsAccessible(std::string filePath, int* exists, int* accessible);

int getFileSize(std::string file);

int writeToSocket(int socket_fd, std::string line);

int writeHeader(int socket_fd, std::string header, std::string header_value);

int write200(int socket_fd);

int write400(int socket_fd, std::string message);

int write400BadHttpVersion(int socket_fd, std::string message);

int write400BadMethod(int socket_fd, std::string message);

int write400BadUri(int socket_fd, std::string message);

int write403(int socket_fd);

int write404(int socket_fd, std::string message);

int write501(int socket_fd, std::string uri);

int writeContentTypeHeader(int socket_fd, std::string content_type);

int writeContentLengthHeader(int socket_fd, int content_length);

int writeKeepAliveHeader(int socket_fd);

void vomitFileToSocketHttp(int socket_fd, std::string fileName, std::string contentType, bool keep_alive);

bool isDirectory(std::string filePath);

#endif /* WEBSERVER_UTIL_H_ */
