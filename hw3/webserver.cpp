/*
 * webserver.cpp
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */

#include "webserver.h"

using namespace std;

Webserver::Webserver(int port=8080, double timeout=10){
	//Assign reasonable defaults to config parameters
	servicePort = port;
	double cacheTimeout = timeout;
}

Webserver::~Webserver(){
}


/**
 * this function will handle a GET request contained in the request variable that is associated
 * with the input socket descriptor. it uses passed in variables for document root, indices and
 * content types. this is likely not going to work on non-linux system due to both the file
 * system calls and how the file path separators are uses. if cross platform support is needed,
 * a higher-level language with platform-independent file system functionality should be used
 * instead of c++
 *
 * this is the only method handling function, as per the email
 * from Sangtae on Sept. 15, 2015:

	Hi all,

	The deadline for the programming assignment #1 is extended to 9/17
	(one week extension). Even you do not know about socket programming,
	you can learn and finish the assignment on time (just start from
	BeeJ's guide). Also don't be shy to post any questions in the
	discussion board. This assignment is the important precursor for the
	other three assignments down the road, so please spend time and
	complete it by yourself.

	Also a few clarifications:

	1) You do not need to implement POST and other requests. You only have
	to implement GET.

	2) after the step (4) in the previous email below, you have to check
	whether your web browser (chrome/firefox/safari) displays the content
	your echo/web server sends. After that, you handle

	3) You may not handle Error Code 501, such as "HTTP/1.1 501 Not
	Implemented: /csc573/video/lecture1.mpg."

	Best,
	Sangtae

 */
static int handleGet(int socket_fd, string request, cacheEntry *entry){
	int exists, accessible;
	string currentContentType;
	bool found = false;

	//tokenize the request to get the method, uri and http version
	char* method = strtok((char*)((request).c_str()), " ");
	char* uri = strtok(NULL, " ");
	char* httpVersion = strtok(NULL, " ");

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
	//check that http version is not null and that it
	//is one that we support. since HTTP 1.0 and 1.1
	//are similar enough for the scope of this assignment
	//support both
	//TODO: FOR PROXY do not support 1.1
	if(httpVersion == NULL){
		//write bad http
		write400BadHttpVersion(socket_fd, "");
		return -1;
	} else if(strncmp(httpVersion, "HTTP/1.0", 8) != 0){
		cout << "TEST-----------------"<<endl;
		cout << httpVersion <<endl;
		write400BadHttpVersion(socket_fd, string(httpVersion));
		return -1;
	}

	string uriString(uri);

	//first, do a DNS query for the URL
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	//strip out protocol and trailing /'s. also check that url is http://,
	//as that is all we support right now
	if(uriString.find("http://") != 0){
		write400BadHttpVersion(socket_fd, "");
		return -1;
	}
	//get rid of the protocol specifier
	uriString.replace(0, 7, "");
	//strip out a trailing slash if it is the last character in string
	if(uriString.back() == '/'){
		uriString.pop_back();
	}

	if ((status = getaddrinfo(uriString.c_str(), "80", &hints, &servinfo)) != 0) {
	    cout << "DNS resolution failed" <<endl;
	    //TODO: send 404
	    write404(socket_fd, uriString);
	    return -1;
	}

	//now open a socket to the ip address
	int dest_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if(dest_fd < 0){
		cout << "Could not create socket" <<endl;
		write404(socket_fd, uriString);
		return -1;
	}

	//now connect to the destination
	if(connect(dest_fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0){
		cout << "Could not connect" << endl;
		write404(socket_fd, uriString);
		return -1;
	}

	writeToSocket(dest_fd, request);

	int bytesReceived = 0;
	char buffer[MAX_REQUEST_SIZE+1];
	string response = "";
	memset(buffer, 0, MAX_REQUEST_SIZE+1);

	while(recv(dest_fd, buffer, MAX_REQUEST_SIZE, 0) > 0){
		response.append(buffer);
		memset(buffer, 0, MAX_REQUEST_SIZE+1);
	}

	entry->data = response;
	entry->timestamp = time(NULL);

	close(dest_fd);
}

static bool lookupInCache(string uri, cacheEntry *entry, map<string, cacheEntry> &contentCache, double timeout){
	time_t currentTime = time(NULL);
	if(contentCache.find(uri) != contentCache.end()){
		cacheEntry currentEntry = contentCache[uri];
		entry->data = currentEntry.data;
		entry->timestamp = currentEntry.timestamp;
		if(difftime(currentTime, currentEntry.timestamp) < timeout){
			return false;
		} else{
			return true;
		}
	}
	return false;
}

static void replyWithEntry(int socket_fd, cacheEntry *entry){
	writeToSocket(socket_fd, entry->data);
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

		//check if queue is empty and whether we should continue
		//TODO: add a sleep time for each check
//		if(args->workQueue->getSize() <= 0){
//			continue;
//		} else{
			//get the first element off the queue
			//skip if for some reason this is null
		//this should block until there is something to get off the queue
		requestItem = args->workQueue->pop();
		if(requestItem == NULL){
			continue;
		}
//		}

		//FOR PROXY: 1st, if the URL is in the cache and in the timeout, return it
		//if the timestamp for the cache entry has not expired
			//if it has expired, remove from cache
		//else, extract out the request URL, do a DNS query, and send a TCP message
		//to the returned IP address. then store the response in the cache with the
		//timestamp of when we store it.
		string request = requestItem->request;
		char tokRequest[request.size()];
		//need to make a copy, as previous implementations had weird errors here
		strcpy(tokRequest, request.c_str());

		//final check of request headers
		char* request_type_str = strtok(tokRequest, " ");
		char* uri = strtok(NULL, " ");
		char* http_version = strtok(NULL, " ");
		if(request_type_str == NULL){
			//invalid method
			write400BadMethod(args->socket_fd, "");
			continue;
		}
		if(uri == NULL){
			cout << "Bad uri. Line: "<<request<<endl;
			write400BadUri(args->socket_fd, "");
			continue;
		}

		if(http_version == NULL){
			write400BadHttpVersion(args->socket_fd, "");
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
				handleGet(args->socket_fd, request, &entry);

			}
			//store in the cache
			contentCache[uriString] = entry;
			//send the enrty that we just looked up or received and send back
			//also closes the socket
			replyWithEntry(args->socket_fd, &entry);
		} else{
			cout << "Not implemented yet" << endl;
			write400BadMethod(args->socket_fd, request_type_str);
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

	/**
	 * keep looping until the timer expires. the timer will expire after
	 * 10 seconds after the first successful read from the socket. if a
	 * keep-alive header is detected then the timer is reset
	 */
	string request;
	while(continueReceving){

		/**
		 * perform a non-blocking read on the buffer
		 * if we read a non-zero # of bytes, then begin timer and
		 * send the request to the worker
		 */
		//clear the receive buffer
		memset(recv_buffer, 0, MAX_REQUEST_SIZE+1);
		//TODO: detect when the end of the request occurs and build up request string until then

		bytesReceived = recv(args->connection_fd, recv_buffer, MAX_REQUEST_SIZE, MSG_DONTWAIT);
		if(bytesReceived > 0){
			char currentReceivedBuffer[bytesReceived];
			memcpy(currentReceivedBuffer, recv_buffer, bytesReceived);

			//append the currently received bytes to the running request string
			request.append(currentReceivedBuffer, bytesReceived);

			//need to check if we received a blank line, as this indicates that the request headers have been read
			//since we do not support a message body in this webserver, can stop there
			char toFind[] = {0xd, 0xa, 0xd, 0xa, 0x0};
			if(request.find(toFind) == request.npos){
				continue;
			}

			/**
			 * check that the method is GET. as per email from Sangtae, we only
			 * need to implement the GET method for this assignment. therefore,
			 * should only put requests that are GET methods onto the work queue
			 */
			//first make sure that the strtok did not fail

			string clone = string(request.c_str());
			char* method = strtok((char*)clone.c_str(), " ");

			//TODO: also check that it is HTTP 1.0
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
				workQueue->push(string(request.c_str()));
				request = "";
			} else{
				/**
				 * send am invalid method message. since strtok is supposed to return
				 * a valid c_str, this should not fail
				 */
				write400BadMethod(args->connection_fd, string(method));
				break;
			}
		}

	}
	pthread_exit(NULL);
}

/**
 *	Runs the HTTP server on the TCP service port provided in the config file
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
	/**
	 * setup worker thread pthread variables and make joinable
	 * need to be joinable so that we can ensure that all work
	 * has been done by the worker before we close the socket
	 * when finished
	 */
	ConcurrentQueue *workQueue = new ConcurrentQueue;

	pthread_t worker_thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	//setup inputs to the worker thread
	struct workerArgs *workerArgsStr = new workerArgs;
	workerArgsStr->workQueue = workQueue;
	workerArgsStr->timeout = cacheTimeout;

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
	cout << "Usage: webserver [-f FILE] [-p PORT] [-h]" << endl;
	cout << "Options:" << endl;
	cout << "  -f FILE\tSpecify a configuration file. If the file cannot be opened, reasonable defaults will be assumed. See README for format. The default is ./ws.conf."<<endl;
	cout << "  -p PORT\tSpecify a service port. Will be ignored if port is set in a configuration file. The default is 8080." <<endl;
	cout << "  -h\t\tPrint this message and exit."<<endl;
}

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
