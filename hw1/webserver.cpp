#include "webserver.h"

using namespace std;

//enum ConfigOptions { ServicePort, DocumentRoot, DirectoryIndex, ContentType };
struct connectionArgs{
	int thread_id;
	int connection_fd;
	string documentRoot;
	set<string> documentIndex;
	map<string, string> contentTypes;
};

struct workerArgs{
	int socket_fd;
	string documentRoot;
	set<string> documentIndex;
	map<string, string> contentTypes;
	ConcurrentQueue *workQueue;
	volatile bool continue_processing;
	volatile bool flush_queue;
	pthread_mutex_t *lock;
};

Webserver::Webserver(string file){
	fileName = file;
	//Assign reasonable defaults to config parameters
	servicePort = 8080;
	documentRoot = ".";
	documentIndex.insert("index.html");
	documentIndex.insert("index.htm");
	contentTypes[".html"] = "text/html";
//	contentTypes[".txt"] = "text/plain";
//	contentTypes[".png"] = "image/png";
//	contentTypes[".gif"] = "image/gif";
	parseFile(fileName);
	cout << "Current configuration options:"<<endl;
	cout << "Service port: " << servicePort << endl;
	cout << "Document root: " << documentRoot << endl;
	for(set<string>::iterator i = documentIndex.begin(); i != documentIndex.end(); i++){
		cout << "Document index: " << *i <<endl;
	}
	for(map<string, string>::iterator i = contentTypes.begin(); i != contentTypes.end(); i++){
		cout << "Content type: " << i->first << " => " << i->second << endl;
	}
}

Webserver::~Webserver(){

}

int Webserver::parseFile(string file){
	cout << "File: "<<file<<endl;
	ifstream configFile(file.c_str());
//	configFile.open(file);
	if(!configFile.is_open()){
		cout << "Error opening config file" << endl;
		return -1;
	}

	set<ConfigOptions> parsedOptions;
	string line;
	ConfigOptions current;
	bool parseSuccess = false;
	int lineCounter = 0;
	while(!configFile.eof()){
		if(getline(configFile, line)){
			cout << line << endl;
			//check if line is a comment or whitespace
			//if(s.find("#") == 0 || iswspace(s)){
				//if so, skip to next line
			//	continue;
			//}
			//check if it is the "Listen" parameter
			if(line.find("Listen") == 0){
				current = ServicePort;
			}
			//check if it is the "DocumentRoot" parameter
			else if(line.find("DocumentRoot") == 0){
				current = DocumentRoot;
			}
			//check if is the "DirectoryIndex" parameter
			else if(line.find("DirectoryIndex") == 0){
				current = DirectoryIndex;
			}
			//check if it is a "Content-Type" option
			else if(line.find(".") == 0){
				current = ContentType;
			}
			//matched none of the options - could be invalid
			//or a comment or whitespace
			else{
				lineCounter++;
				continue;
			}
			parseSuccess = parseOption(line, current);
			if(!parseSuccess){
				cout << "Could not parse option: " + current << " at line: " << lineCounter << endl;
			}
		}
		lineCounter++;
	}
	return 0;
}

//parse a string assuming it is the option specified by option
bool Webserver::parseOption(string line, ConfigOptions option){
//	cout << "Parsing" <<endl;
	switch(option){
		case ServicePort:
		{
		//	cout << "Listen parsing " << endl;
			int port;
			if(sscanf(line.c_str(), "Listen %i", &port) == EOF){
				cout << "Could not parse service port" << endl;
				return false;
			}
			cout << "Listen port: " << port << endl;
			servicePort = port;
			//break;
			return true;
		}
		case DocumentRoot:
		{
			char docBuffer[1024];
			if(sscanf(line.c_str(), "DocumentRoot %s", docBuffer) == EOF){
				cout << "could not parse Document Root" << endl;
				return false;
			}
			documentRoot = string(docBuffer);
			int firstOccurence = documentRoot.find("\"");
			documentRoot = documentRoot.replace(firstOccurence, firstOccurence + 1, "");
			int secondOccurrence = documentRoot.find("\"");
			documentRoot = documentRoot.replace(secondOccurrence, secondOccurrence + 1, "");
			cout << "Replaced document root: " << documentRoot << endl;
		//	break;
			return true;
		}
		case DirectoryIndex:
		{
			char* tok = strtok((char*)line.c_str(), " ");
			if(strcmp(tok, "DirectoryIndex") != 0){
				cout << "Could not parse Directory Index" << endl;
				return false;
			}
			while(tok != NULL){
				if(strcmp(tok, "DirectoryIndex") != 0){
					documentIndex.insert(tok);
				}
				tok = strtok(NULL, " ");
			}
		///	break;
			return true;
		}
		case ContentType:
		{
			if(line.find(".") != 0){
				cout << "Could not parse content type" << endl;
				return false;
			}
			char fileType[1024];
			char contentType[1024];
			if(sscanf(line.c_str(), "%s %s", fileType, contentType) == EOF){
				cout << "Could not parse content type" << endl;
				return false;
			}
			contentTypes[fileType] = contentType;
			//break;
			return true;
		}
	}
	return false;
}

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
//	cout << "Writing line to socket: " << line << endl;
	int written = write(socket_fd, line.c_str(), line.length());
	written += write(socket_fd, "\n", strlen("\n"));
	return written;
}

int writeHeader(int socket_fd, string header, string header_value){
	string total_line = header.append(header_value);
	return writeToSocket(socket_fd, total_line);
}

int write200(int socket_fd){
	return writeHeader(socket_fd, "HTTP/1.1 200 OK" , "");
}

int write400(int socket_fd, string message){
	return writeHeader(socket_fd, "HTTP/1.1 400 Bad Request: ", message);
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
	return writeHeader(socket_fd, "HTTP/1.1 404 Not Found: ", message);
}

int write501(int socket_fd, string uri){
	return writeHeader(socket_fd, "HTTP/1.1 501 Not Implemented: ", uri);
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

static void vomitFileToSocketHttp(int socket_fd, string fileName, string contentType, bool keep_alive){
	string header;
	//send a 200: ok header to socket
//	cout << "Sending 200" << endl;
	write200(socket_fd);
	//send a content type header
//	cout << "Sending content type header: " << contentType << endl;
	writeContentTypeHeader(socket_fd, contentType);
	//calculate the length of the file and send a length header
//	cout << "Sending content length header: " << getFileSize(fileName) << endl;
	int fileSize = getFileSize(fileName);
	writeContentLengthHeader(socket_fd, fileSize);
	if(keep_alive){
		//send a keep-alive header
//		cout << "Sending kep alive header" << endl;
		writeKeepAliveHeader(socket_fd);
	}

	writeToSocket(socket_fd, "");
	//loop through file and send contents
	ifstream file(fileName.c_str());
	string line;
//	cout << "Writing file: " << fileName << endl;
	int written = 0;
	while(!file.eof()){
		if(getline(file, line)){
//			cout << "Current line of "
			written += writeToSocket(socket_fd, line);
		}
	}
	cout << "Wrote " << written << " bytes" <<endl;
	while(written++ < fileSize){
		writeToSocket(socket_fd, " ");
	}
	cout << "Wrote " << written << " bytes" <<endl;
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

static void handleGet(int socket_fd, string request, string documentRoot, set<string> documentIndex, map<string, string> contentTypes, bool keep_alive){
	cout << "In handle get" << endl;

	int i;
	cout << "Request: " <<request << endl;
	printf("\n");
	for(i=0; i<20; i++){
		printf(" %02x ", request.c_str()[i]);
	}
	cout << request << endl;
	cout << "In handle get 2" << endl;
	char* tok = strtok((char*)((request).c_str()), " ");
	char* uri = strtok(NULL, " ");
	string filePath = documentRoot;
	cout << "Document root: " << documentRoot << endl;
	filePath = filePath.append("/");
	//cout << "Document root appended: " << filePath << endl;
	if(uri == NULL){
//		cout << "Dying. Request: " <<request<<endl;
//		exit(-1);
		//write bad uri
		cout << "Wrote 400" <<endl;
		write400BadUri(socket_fd, "");
		return;
	}
	filePath = filePath.append(uri);
	string currentContentType;
	bool found = false;
	int exists;
	int accessible;
	checkFilepathExistsAccessible(filePath, &exists, &accessible);
	string contentType = "";
	cout << "Is " << filePath << " a directory? " << isDirectory(filePath) << endl;
	if(!isDirectory(filePath) && (exists == 1 && accessible == 1)){
		//need to determine the content type and then vomit over the socket
		found = true;
		//need to check the content types. if the uri has a file extension, then
		//can iterate through the content types. if not, then is an index and should continue. if has a file type and does not
		//match an entry in the content-types, send 501: not implemented
		cout << "looking for content type for uri: " << filePath << endl;
		for(map<string, string>::iterator i = contentTypes.begin(); i != contentTypes.end(); i++){
			//cout << "Content type: " << i->first << " => " << i->second << endl;
			if(endsWith((char*)(filePath.c_str()), (char*)((i->first).c_str()))){
				contentType = i->second;
				cout << "Current content type: " << i->second << endl;
			}
		}
		if(contentType == ""){
			write501(socket_fd, string(uri));
		} else{
			vomitFileToSocketHttp(socket_fd, filePath, contentType, keep_alive);
		}
		return;
	}
	if(isDirectory(filePath) || (exists == 0 && accessible == 0)){
		//need to try the documentIndex's
		cout << "looking for index for uri: " << filePath << endl;
		filePath = filePath.append("/");
		for(set<string>::iterator i = documentIndex.begin(); i != documentIndex.end(); i++){
//			cout << "Current index: " << *i <<endl;
			string currentIndex = string(filePath).append(*i);
			cout << "Current index string: " << currentIndex << endl;
			checkFilepathExistsAccessible(currentIndex, &exists, &accessible);
			if(exists == 1 && accessible == 1){
				found = true;
				//vomit the index over the socket with content type = html
				cout << "Vomiting index file: " << currentIndex <<endl;
				vomitFileToSocketHttp(socket_fd, currentIndex, "text/html", keep_alive);
				return;
			}
		}

	}
	if(!found){
		//need to send an error code. may need to store the output of the access call
		cout << "could not find uri" << endl;
		cout << "Exists: " << exists << " Accessible: " << accessible << endl;
		if(exists == 0){
			//send 404
			cout << "Writing 404" << endl;
			write404(socket_fd, string(uri));
		} else if(exists == 1 && accessible == 0){
			//send 403
		}
	}

}

static void* workerThreadTask(void* workerArgs){
	struct workerArgs *args = (struct workerArgs*)(workerArgs);
	int requestType;
	//flush queue will ensure that we empty the work queue before terminating
	cout << "Beginning worker thread" <<endl;
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;
	pthread_mutex_lock(args->lock);
	volatile bool localContinue = args->continue_processing;
	volatile bool localFlush = args->flush_queue;
	pthread_mutex_unlock(args->lock);
	while(localFlush || localContinue){
//		cout << "Beginning worker iteration. Continue: " << localContinue << " flush: " << localFlush << endl;
		bool skip = false;
		struct QueueItem* requestItem;
		if(args->workQueue->getSize() <= 0){

			if(localContinue){
				skip = true;
			} else{
				break;
			}
		} else{
			cout << "Popping" << endl;
			requestItem = args->workQueue->pop();
			cout << "Popped" << endl;
			if(requestItem == NULL){
				skip = true;
			}
		}
		if(!skip){
			string request = requestItem->request;
			cout << "Request after pop: " << request <<endl;
			char tokRequest[request.size()];
			strcpy(tokRequest, request.c_str());
			char* request_type_str = strtok(tokRequest, " ");
			cout << "Request after strtok: " << request <<endl;
			char* uri = strtok(NULL, " ");
			char* http_version = strtok(NULL, " ");
			if(request_type_str == NULL){
				//invalid method
				write400BadMethod(args->socket_fd, "");
				continue;
			}
			if(uri == NULL){
				write400BadUri(args->socket_fd, "");
				continue;
			}
			if(http_version == NULL){
				write400BadHttpVersion(args->socket_fd, "");
				continue;
			}
			if(strncmp(request_type_str, "GET", 3) == 0){
				requestType = 0;
			}
			if(requestType == 0){
				cout << "Handling GET request. Request: " << request.c_str() <<endl;
				bool keep_alive= false;
				if(request.find("keep-alive") != request.npos){
					keep_alive = true;
				}
				handleGet(args->socket_fd, request, args->documentRoot, args->documentIndex, args->contentTypes, keep_alive);
			} else{
				cout << "Not implemented yet" << endl;
			}
		}
		pthread_mutex_lock(args->lock);
		localContinue = args->continue_processing;
		localFlush = args->flush_queue;
		pthread_mutex_unlock(args->lock);
		nanosleep(&sleepTime, &sleepTimeRem);
	}
	pthread_exit(NULL);
}

static void *handleConnection(void* connectionArgs){
	char recv_buffer[1000];
	struct connectionArgs *args = (struct connectionArgs*)(connectionArgs);
	int bytesReceived;
	cout << "Started receive thread" << endl;
	int type = 0;
	int i;
	time_t start;
	bool waitForTimer = false;
//	volatile bool keep_worker_working = true;
//	volatile bool flush_queue;
	pthread_t worker_thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	ConcurrentQueue workQueue;


	pthread_mutex_t* lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));

	pthread_mutex_init(lock, NULL);

	cout << "Here!" <<endl;

	struct workerArgs *workerArgsStr = new workerArgs;
	workerArgsStr->contentTypes = args->contentTypes;
	workerArgsStr->continue_processing = true;//&keep_worker_working;
	workerArgsStr->documentIndex = args->documentIndex;
	workerArgsStr->documentRoot = args->documentRoot;
	workerArgsStr->socket_fd = args->connection_fd;
	workerArgsStr->workQueue = &workQueue;
	workerArgsStr->flush_queue = false;//&flush_queue;
	workerArgsStr->lock= lock;



	pthread_create(&worker_thread, &attr, workerThreadTask, (void*)workerArgsStr);

	start = time(NULL);//clock();
	bool continueReceving = true;
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;
	list<string> requests;
	bool isFirst = true;
	while(continueReceving){
		//check buffer
		bytesReceived = recv(args->connection_fd, recv_buffer, 1000, MSG_DONTWAIT);
		if(bytesReceived > 0){
			recv_buffer[999] = '\0';

			char* tok = strtok(recv_buffer, " ");
			//need to check HTTP version and send 400: bad http version before we process the request. should be the 3rd token
			//need to check for keep alive
			bool keep_alive = false;

			for(i=0; i<999; i++){
				if(recv_buffer[i] == '\0'){
					recv_buffer[i] = ' ';
				}
			}

			string request = string(recv_buffer);
			requests.push_back(request);
			cout << "Received: " << request << endl;

			if(request.find("Connection: keep-alive") != request.npos){
				keep_alive = true;
				waitForTimer = true;
				start = time(NULL);//clock();
				cout << "Resetting timer" << endl;
			} else if(bytesReceived > 0 && isFirst){
				waitForTimer = true;
				start = time(NULL);//clock();
				isFirst = false;
			}
			else{
				waitForTimer = false;
			}

			cout << "Pushing" << endl;
			workQueue.push(request, keep_alive);
			cout << "Pushed" <<endl;

			printf("\nFirst 32 bytes of buffer: ");
			for(i=0; i<16; i++){
				printf(" %02x ", recv_buffer[i]);
			}
		}
		if(waitForTimer){
			cout << "Waiting----------------------------------------------------------------------------------"<<endl;
//			clock_t current = time(NULL);//clock();
			time_t current = time(NULL);
			double difference = difftime(current, start);//((double)current - (double)start);
			double seconds = difference;// / CLOCKS_PER_SEC;
			if(seconds >= 10){
				//timer expired
				continueReceving = false;
				cout << "Timer expired. Seconds: " << seconds << endl;
			} else{
				cout << "Waiting. Seconds: " << seconds << endl;
			}
		}
//		sleep(50);
		nanosleep(&sleepTime, &sleepTimeRem);

	}
	cout << "Waiting for worker to terminate" << endl;
//	flush_queue = true;
//	keep_worker_working = false;
	pthread_mutex_lock(lock);
	workerArgsStr->continue_processing = false;
	workerArgsStr->flush_queue = true;
	void* status;
	workQueue.toggleQueue();
	workQueue.wakeQueue();
	pthread_mutex_unlock(lock);
	pthread_join(worker_thread, &status);
	cout << "Closing socket" << endl;
	close(args->connection_fd);
	pthread_mutex_destroy(lock);
	free(lock);
	pthread_exit(NULL);
}

int Webserver::runServer(){
	int socket_fd, status, new_connection_fd, rc, thread_ids = 0;
	list<pthread_t> threads;
	struct sockaddr_in server_addr;
	char service[100];
	sprintf(service, "%i", servicePort);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(servicePort);

	cout << "Running server" << endl;
	cout << "Service port: " << string(service) <<endl;

//	status = getaddrinfo(NULL, service, &server_info, &addr_info_list);
//	if(status != 0){
//		cout << "Error getting address info" << endl;
//		return -1;
//	}

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		return socket_fd;
	}

	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	status = bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0){
		cout << "Error binding on socket" << endl;
		return status;
	} else{
		cout << "Running tcp server on port: "<<servicePort<<endl;
	}

	status = listen(socket_fd, 10);
	if(status < 0){
		cout << "Error listening on bound socket" << endl;
		return status;
	} else{
		cout << "Listenting on tcp socket" << endl;
	}

	while(true){
		char clientIp[INET_ADDRSTRLEN];
		cout << "Accepting new connections" << endl;
		struct sockaddr connection_addr;
		socklen_t connection_addr_size = sizeof(sockaddr);
		pthread_t current_connection_thread;
		struct connectionArgs *args = new connectionArgs;
		new_connection_fd = accept(socket_fd, &connection_addr, &connection_addr_size);\
		int ip = ((struct sockaddr_in*)(&connection_addr))->sin_addr.s_addr;
		inet_ntop(AF_INET, &ip, clientIp, INET_ADDRSTRLEN);
		cout << "Accepted new connection from: " << string(clientIp) << endl;
		args->thread_id = thread_ids;
		args->connection_fd = new_connection_fd;
		cout << "Document root before pthread: " << documentRoot <<endl;
		args->documentRoot = documentRoot;
		args->documentIndex = documentIndex;
		args->contentTypes= contentTypes;
		rc = pthread_create(&current_connection_thread, NULL, handleConnection, (void*)args);
		if(rc){
			cout << "Error creating thread: "<<thread_ids<<endl;
			return -1;
		}
		thread_ids++;
		threads.push_back(current_connection_thread);
	}

}






int main(){
	Webserver server("/home/michael/network_systems/hw1/ws.conf");
	server.runServer();
	return 0;
}










