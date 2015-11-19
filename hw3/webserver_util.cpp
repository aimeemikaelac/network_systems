/*
 * webserver_util.cpp
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */
#include "webserver_util.h"

using namespace std;

//1 if str ends with suffix, 0 otherwise
bool endsWith(char* str, char* suffix){
	if(!str || !suffix){
		return 0;
	}
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix >  lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void checkFilepathExistsAccessible(string filePath, int* exists, int* accessible){
	if(access(filePath.c_str(), F_OK) == 0){
		*exists = 1;
	} else{
		*exists = 0;
		*accessible = 0;
		return;
	}
	if(access(filePath.c_str(), R_OK) == 0){
			*accessible = 1;
	} else{
		*accessible = 0;
		return;
	}
}

int getFileSize(string file){
//	struct stat stat_buf;
//	int rc = stat(file.c_str(), &stat_buf);
//	return rc == 0? stat_buf.st_size : -1;
	FILE *p_file = NULL;
	p_file = fopen(file.c_str(),"rb");
	fseek(p_file,0,SEEK_END);
	int size = ftell(p_file);
	fclose(p_file);
	return size;
}

int writeToSocket(int socket_fd, string line){
	int written = write(socket_fd, line.c_str(), line.length());
	written += write(socket_fd, "\n", strlen("\n"));
	return written;
}

int writeCharToSocket(int socket_fd, unsigned char* buffer, int bufferLength){
	int written = write(socket_fd, buffer, bufferLength);
	return written;
}

int writeHeader(int socket_fd, string header, string header_value){
	string total_line = header.append(header_value);
	return writeToSocket(socket_fd, total_line);
}

int write200(int socket_fd){
	return writeHeader(socket_fd, "HTTP/1.0 200 OK" , "");
}

int write400(int socket_fd, string message){
	return writeHeader(socket_fd, "HTTP/1.0 400 Bad Request: ", message.append("\n"));
}

int write400BadHttpVersion(int socket_fd, string message){
	string codeMessage = "Invalid HTTP-Version: ";
	return write400(socket_fd, codeMessage.append(message));
}

int write400BadMethod(int socket_fd, string message){
	string codeMessage = "Invalid Method: ";
	return write400(socket_fd, codeMessage.append(message));
}

int write400BadUri(int socket_fd, string message){
	string codeMessage = "Invalid URI: ";
	return write400(socket_fd, codeMessage.append(message));
}

int write404(int socket_fd, string message){
	cout << "404 message: "<<message<<endl;
	return writeHeader(socket_fd, "HTTP/1.0 404 Not Found: ", message.append("\n"));
}

int write403(int socket_fd){
	//always forbidden, since we don't provide basic auth
	return writeHeader(socket_fd, "HTTP/1.0 403 Forbidden", "\n");
}

int write501(int socket_fd, string uri){
	return writeHeader(socket_fd, "HTTP/1.0 501 Not Implemented: ", uri.append("\n"));
}

int writeContentTypeHeader(int socket_fd, string content_type){
	return writeHeader(socket_fd, "Content-Type: ", content_type);
}

int writeContentLengthHeader(int socket_fd, int content_length){
	char buf[100];
	sprintf(buf, "%i", content_length);
	return writeHeader(socket_fd, "Content-Length: ", string(buf));
}

int writeKeepAliveHeader(int socket_fd){
	return writeHeader(socket_fd, "Connection: keep-alive", "");
}

void vomitFileToSocketHttp(int socket_fd, string fileName, string contentType, bool keep_alive){
	string header;
	//send a 200: ok header to socket
	write200(socket_fd);
	//send a content type header
	writeContentTypeHeader(socket_fd, contentType);
	//calculate the length of the file and send a length header
	int fileSize = getFileSize(fileName);
	writeContentLengthHeader(socket_fd, fileSize);
	if(keep_alive){
		//send a keep-alive header
		writeKeepAliveHeader(socket_fd);
	}

	writeToSocket(socket_fd, "");
	//loop through file and send contents
	ifstream file(fileName.c_str());
	string line;
	int written = 0;
	while(!file.eof()){
		if(getline(file, line)){
			written += writeToSocket(socket_fd, line);
		}
	}
	while(written++ < fileSize){
		writeToSocket(socket_fd, " ");
	}
}

bool isDirectory(string filePath){
	struct stat s;
	if(stat(filePath.c_str(), &s) == 0){
		if(s.st_mode & S_IFDIR){
			return true;
		}
	}
	return false;
}
