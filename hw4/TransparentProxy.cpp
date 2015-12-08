/*
 * TransparentProxy.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: michael
 */

#include "TransparentProxy.h"

using namespace std;

TransparentProxy::TransparentProxy() {
	// TODO Auto-generated constructor stub

}

TransparentProxy::~TransparentProxy() {
	// TODO Auto-generated destructor stub
}

static void* workerThreadTask(void *workerArgsStruct){

}

static void* handleConnection(void *handlerArgsStruct){
	struct connectionArgs *handlerArgs= (struct connectionArgs*)(handlerArgsStruct);
	struct sockaddr_in clientSrcPortInfo;
	socklen_t clientInfoLen;
	if(getsockname(handlerArgs->connection_fd, (sockaddr *)&clientSrcPortInfo, &clientInfoLen) < 0){
		cout << "Could not get client src port information" << endl;
	}
	int clientSrcPort = clientSrcPortInfo.sin_port;
	struct sockaddr_in clientSrcIpInfo;
	if(getpeername(handlerArgs->connection_fd, (sockaddr *)(&clientSrcIpInfo), &clientInfoLen) < 0){
		cout << "Could not get client src IP information" << endl;
	}
	string clientSrcIp = string(inet_ntoa(clientSrcIpInfo.sin_addr));
	struct sockaddr_in clientOriginalDst;
	if(getsockopt(handlerArgs->connection_fd, SOL_IP, SO_ORIGINAL_DST, &clientOriginalDst, &clientInfoLen) < 0){
		cout << "Could not get original client destination"<<endl;
	}
}


/**
 * Runs a proxy that forwards requests from ip1 to ip2 and then routes backwards
 */
int runProxy(string clientSide, string serverSide, int clientSidePort){
	//need to listen on the clientSide ip address
	//any connections will be put onto queue by a thread
	//a worker will process the work on the queue
	int socket_fd, status, new_connection_fd, rc, thread_ids = 0;
		list<pthread_t*> threads;
		struct sockaddr_in server_addr;
		char service[100];
		sprintf(service, "%i", clientSidePort);

		//setup struct to create socket
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(clientSide.c_str());
		server_addr.sin_port = htons(clientSidePort);

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
			cout << "Running HTTP server on TCP port: "<<clientSidePort<<endl;
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
