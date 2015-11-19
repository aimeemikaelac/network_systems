/*
 * webserver.cpp
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */

#include "webserver.h"

using namespace std;

Webserver::Webserver(int port=8080, double timeout=10.0){
	//Assign reasonable defaults to config parameters
	servicePort = port;
	double cacheTimeout = timeout;
	cout << "Timeout: "<<timeout<<endl;
	cout << "Cache timeout: "<<cacheTimeout<<endl;
}

Webserver::~Webserver(){
}


/**
 * this function will handle a GET request from a proxy client to the proxy server.
 * this is only executed when there is not a valid entry in the cache.
 * will perform a DNS query to the URL of the destination in the HTTP GET request and
 * then will send the client's request in a TCP message unmodified to this destination
 */
static int handleGet(int socket_fd, string request, cacheEntry *entry){
	int exists, accessible;
	string currentContentType;
	bool found = false;

	//tokenize the request to get the method, uri and http version
	char requestCopy[request.length()];
	memcpy(requestCopy, request.c_str(), request.length());
	char* method = strtok((char*)(requestCopy), " ");
	char* uri = strtok(NULL, " ");
	char* httpVersion = strtok(NULL, " ");

	//set up the variables for the sleep function
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;

	/**
	 * make sure that these values are valid and are what is
	 * required and what we support for this assignment
	 * this should be caught before now, but check again to
	 * be safe
	 */
	if(method == NULL){
		write400BadMethod(socket_fd, "");
		return -1;
	} else if(strncmp(method, "GET", 3) != 0){
		write400BadMethod(socket_fd, string(method));
	}
	if(uri == NULL){
		//write bad uri
		write400BadUri(socket_fd, "");
		return -1;
	}
	/**
	 * check that http version is not null and that it
	 * is one that we support.
	 */
	if(httpVersion == NULL){
		//write bad http
		write400BadHttpVersion(socket_fd, "");
		cout << "HTTP version null" << endl;
		return -1;
	} else if(strncmp(httpVersion, "HTTP/1.0", 8) != 0){
		cout << "TEST-----------------"<<endl;
		cout << httpVersion <<endl;
		write400BadHttpVersion(socket_fd, string(httpVersion));
		return -1;
	}

	string uriString(uri);
	string uriDnsString(uri);

	//first, do a DNS query for the URL
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	//get a URL for DNS lookup
	//get rid of the protocol specifier
	if(uriDnsString.find("http://") == 0){
		uriDnsString.replace(0, 7, "");
	}
	/**
	 * strip out everything after and including the first forward slash
	 * to get the base URL
	 */
	uriDnsString = uriDnsString.substr(0, uriDnsString.find("/"));

	cout << "DNS lookup on: "<<uriDnsString<<endl;
	if ((status = getaddrinfo(uriDnsString.c_str(), "80", &hints, &servinfo)) != 0) {
	    cout << "DNS resolution failed" <<endl;
	    //TODO: send 404
	    write404(socket_fd, uriDnsString);
	    return -1;
	}
	cout << "DNS entries for "<<uriDnsString<<":"<<endl;
	struct addrinfo *p;
	struct sockaddr_in *h;
	for(p= servinfo; p!=NULL; p = p->ai_next){
		h = (struct sockaddr_in *)p->ai_addr;
		char *uriIp = inet_ntoa(h->sin_addr);

		string ip(uriIp);
		cout << ip<<endl;
	}

	//now open a socket to the ip address
	int dest_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if(dest_fd < 0){
		cout << "Could not create socket" <<endl;
		write404(socket_fd, uriString);
		return -1;
	}

	//strip out a keep-alive header
	string keepAlive("Connection: keep-alive");
	if(request.find(keepAlive) != request.npos){
		request.replace(request.find(keepAlive), keepAlive.length(), "");
	}

	//now connect to the destination
	if(connect(dest_fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0){
		cout << "Could not connect" << endl;
		write404(socket_fd, uriString);
		return -1;
	}
//	cout << "Request:------------------------------"<<endl << "--------------------------"<<endl;
//	cout << request<<endl;

	//write the request from the client to the destination
	writeToSocket(dest_fd, request);
	writeToSocket(dest_fd, "\n\n\n");

	//receive any response from the destination
	int bytesReceived = 0;
	unsigned char buffer[MAX_REQUEST_SIZE+1];
	unsigned char *response = NULL;
	memset(buffer, 0, MAX_REQUEST_SIZE+1);

	int received = 0;
	int length = received;
	while((received = recv(dest_fd, buffer, MAX_REQUEST_SIZE, 0)) > 0){
		response = (unsigned char*)realloc(response, length + received + 1);
		memcpy(response + length, buffer, received);
		length += received;
		response[length] = '\0';
	}

	entry->data = response;
	entry->dataLength = length;
	entry->timestamp = time(NULL);

	free(servinfo);
	close(dest_fd);
}

static bool lookupInCache(string uri, cacheEntry *entry, map<string, cacheEntry> &contentCache, double timeout){
	time_t currentTime = time(NULL);
	if(contentCache.find(uri) != contentCache.end()){
		cacheEntry currentEntry = contentCache[uri];
		double difference;
		if((difference = difftime(currentTime, currentEntry.timestamp)) > timeout){
			cout << "Time difference: "<<difference<<endl;
			cout << "Timeout: "<<timeout<<endl;
			free(currentEntry.data);
			contentCache.erase(uri);
			return false;
		} else{
			memcpy(entry, &currentEntry, sizeof(cacheEntry));
			return true;
		}
	}
	return false;
}

static void replyWithEntry(int socket_fd, cacheEntry *entry){
	cout << "Reply length: "<<entry->dataLength<<endl;
	writeCharToSocket(socket_fd, entry->data, entry->dataLength);
	close(socket_fd);
}

/**
 * The function that is executed by a worker thread
 */
static void* workerThreadTask(void* workerArgs){
	struct workerArgs *args = (struct workerArgs*)(workerArgs);
	map<string, cacheEntry> contentCache;
	int requestType = -1;

	//set up the variables for the sleep function
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;

	double timeout = args->timeout;

	//loop till the program exits
	while(true){
		//flag to skip writing if there is nothing in the queue
		struct QueueItem* requestItem;

		//this should block until there is something to get off the queue
		requestItem = args->workQueue->pop();
		if(requestItem == NULL){
			nanosleep(&sleepTime, &sleepTimeRem);
			continue;
		}

		/**
		 * 1st, if the URL is in the cache and in the timeout, return it
		 * if the timestamp for the cache entry has not expired
		 * else, extract out the request URL, do a DNS query, and send a TCP message
		 * to the returned IP address. then store the response in the cache with the
		 * timestamp of when we store it.
		*/
		string request = requestItem->request;
		int connection_fd = requestItem->socket_fd;
		char tokRequest[request.size()];
		//need to make a copy, as previous implementations had weird errors here
		strcpy(tokRequest, request.c_str());

		//final check of request headers
		char* request_type_str = strtok(tokRequest, " ");
		char* uri = strtok(NULL, " ");
		char* http_version = strtok(NULL, " ");
		if(request_type_str == NULL){
			//invalid method
			write400BadMethod(connection_fd, "");
			continue;
		}
		if(uri == NULL){
			cout << "Bad uri. Line: "<<request<<endl;
			write400BadUri(connection_fd, "");
			continue;
		}

		if(http_version == NULL){
			write400BadHttpVersion(connection_fd, "");
			continue;
		}

		//check that this is the correct method
		if(strncmp(request_type_str, "GET", 3) == 0){
			requestType = 0;
		}

		//if is GET, then process it. else, send 400 bad method
		if(requestType == 0){
			cacheEntry entry;
			string uriString = string(uri);
			bool validInCache = lookupInCache(uriString, &entry, contentCache, timeout);
			if(!validInCache){
				cout << "Cache entry does not exist or expired. Re-acquiring" <<endl;
				handleGet(connection_fd, request, &entry);
			} else{
				cout << "Serving page from cache" <<endl;
			}
			//store in the cache
			contentCache[uriString] = entry;
			//send the entry that we just looked up or received and send back
			//also closes the socket
			replyWithEntry(connection_fd, &entry);
		} else{
			cout << "Not implemented yet" << endl;
			write400BadMethod(connection_fd, request_type_str);
		}
		free(requestItem);
	}
	pthread_exit(NULL);
}

/**
 *	Function to be executed by a pthread to handle a connection. Connection
 *	is passed as a file descriptor in the struct. This function will spawn
 *	a separate worker thread. Will then read from the socket at intervals
 *	and put data into a queue to be processed. This allows for the pipelining
 *	to be performed easily
 */
static void *handleConnection(void* connectionArgs){
	char recv_buffer[MAX_REQUEST_SIZE+1];
	struct connectionArgs *args = (struct connectionArgs*)(connectionArgs);
	int bytesReceived;
	time_t start;
	bool waitForTimer = false, continueReceving = true, isFirst = true;

	ConcurrentQueue *workQueue = args->workQueue;

	//set up the variables for the sleep function
	struct timespec sleepTime;
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 500000000; //0.5 seconds in ns
	struct timespec sleepTimeRem;


	/**
	 * keep looping until we have received a GET request or the socket is closed
	 * or something we don't handle is received
	 */
	string request;
	while(continueReceving){

		/**
		 * perform a non-blocking read on the buffer
		 */
		//clear the receive buffer
		memset(recv_buffer, 0, MAX_REQUEST_SIZE+1);

		bytesReceived = recv(args->connection_fd, recv_buffer, MAX_REQUEST_SIZE, MSG_DONTWAIT);
		if(bytesReceived > 0){
			char currentReceivedBuffer[bytesReceived];
			memcpy(currentReceivedBuffer, recv_buffer, bytesReceived);

			//append the currently received bytes to the running request string
			request.append(currentReceivedBuffer, bytesReceived);

			/**
			 * check that the method is GET
			 */
			string clone = string(request.c_str());
			char* method = strtok((char*)clone.c_str(), " ");

			//TODO: explicitly check if the socket is still open
			if(method == NULL){
				//if so, send bad method
				write400BadMethod(args->connection_fd, "");
				break;
			} else if(strncmp(method, "GET", 3) == 0){
				/**
				 * put the request onto the work queue.
				 * should notify the worker thread that
				 * data is available automatically
				 *
				 * make a deep copy of the string so that
				 * we can reset the request
				 */
				workQueue->push(string(request.c_str()), args->connection_fd);
				request = "";
				break;
			} else{
				/**
				 * send am invalid method message. since strtok is supposed to return
				 * a valid c_str, this should not fail
				 */
				write400BadMethod(args->connection_fd, string(method));
				break;
			}
		}
		nanosleep(&sleepTime, &sleepTimeRem);

	}
	pthread_exit(NULL);
}

/**
 *	Runs the proxy server on the configured service port
 */
int Webserver::runServer(){
	int socket_fd, status, new_connection_fd, rc, thread_ids = 0;
	list<pthread_t*> threads;
	struct sockaddr_in server_addr;
	char service[100];
	sprintf(service, "%i", servicePort);

	//setup struct to create socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(servicePort);

	cout << "Running server" << endl;

	//request socket from system
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		return socket_fd;
	}

	//set socket reusable from previous program run
	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	//bind to socket
	status = bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0){
		cout << "Error binding on socket" << endl;
		return status;
	} else{
		cout << "Running HTTP server on TCP port: "<<servicePort<<endl;
	}

	//listen on socket with a backlog of 10 connections
	status = listen(socket_fd, 100);
	if(status < 0){
		cout << "Error listening on bound socket" << endl;
		return status;
	} else{
		cout << "Listening on TCP socket" << endl;
	}

	//FOR PROXY: create a single worker thread instead of a worker for each connection
	ConcurrentQueue *workQueue = new ConcurrentQueue;

	pthread_t worker_thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	//setup inputs to the worker thread
	struct workerArgs *workerArgsStr = new workerArgs;
	workerArgsStr->workQueue = workQueue;
	cout << "Cache timeout: "<<(double)cacheTimeout<<"---------------------------"<<endl;
	workerArgsStr->timeout = 10;

	/**
	 * spawn the worker thread immediately so that is ready for work
	 * when we read from the socket
	 */
	pthread_create(&worker_thread, &attr, workerThreadTask, (void*)workerArgsStr);


	while(true){
		//accept connections until program terminated
		char clientIp[INET_ADDRSTRLEN];
		cout << "Accepting new connections" << endl;

		struct sockaddr connection_addr;
		socklen_t connection_addr_size = sizeof(sockaddr);

		//block on accept until new connection
		new_connection_fd = accept(socket_fd, &connection_addr, &connection_addr_size);
		if(new_connection_fd < 0){
			cout << "Error accepting connection" <<endl;
		}

		//calculate ip of client
		int ip = ((struct sockaddr_in*)(&connection_addr))->sin_addr.s_addr;
		inet_ntop(AF_INET, &ip, clientIp, INET_ADDRSTRLEN);
		cout << "Accepted new connection from: " << string(clientIp) << " in socket: " << new_connection_fd << endl;

		//spin off new pthread to handle connection

		//setup thread and inputs
		pthread_t* current_connection_thread = (pthread_t*)malloc(sizeof(pthread_t));
		struct connectionArgs *args = new connectionArgs;
		args->thread_id = thread_ids;
		args->connection_fd = new_connection_fd;
		args->workQueue = workQueue;

		//spawn thread. we do not wait on these, since we do not care when they finish
		//these threads handle closing the socket themselves
		rc = pthread_create(current_connection_thread, NULL, handleConnection, (void*)args);
		if(rc){
			cout << "Error creating thread: "<<thread_ids<<endl;
			return -1;
		}
		thread_ids++;
		threads.push_back(current_connection_thread);
	}
	return 0;
}

void printHelp(){
	cout << "Usage: proxyserver <port> <timeout>" << endl;
	cout << "Options:" << endl;
	cout << "  <port> - service port"<<endl;
	cout << "  <timeout> - cache timeout, in seconds" <<endl;
}cacheTimeout

int main(int argc, char** argv){
	int port = 8080;
	double timeout = 10;
	if(argc >= 2){
		port = atoi(argv[1]);
	}
	if(argc >= 3){
		timeout = atof(argv[2]);
	}
	Webserver server(port, timeout);
	cout << "Return code: " << server.runServer() << endl;
	return 0;
}
