/*
 * ConcurrentQueue.h
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */

#ifndef CONCURRENTQUEUE_H_
#define CONCURRENTQUEUE_H_

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "util.h"

struct QueueItem{
	std::string user;
	std::string password;
	RequestOption command;
	std::string file;
	std::string fileContents;
	int length;
	int socket;
	QueueItem *next;
};

struct Queue{
	//adapted from locked queue from book, chapter 8
	pthread_mutex_t *lock;
	pthread_cond_t *notFull;
	pthread_cond_t *notEmpty;
	struct QueueItem *head;
	struct QueueItem *tail;
	int count;
	int keepAlive;
};

typedef struct Queue Queue;
typedef struct QueueItem QueueItem;

class ConcurrentQueue {
public:
	ConcurrentQueue();
	virtual ~ConcurrentQueue();
	QueueItem* pop();
	void push(RequestOption command, std::string user, std::string password,
			std::string file, std::string fileContents, int length, int socket);
	int getSize();
	void toggleQueue();
	void wakeQueue();
private:
	void enq(QueueItem* start, QueueItem* end, int newItems, Queue *queue);
	QueueItem* deq(Queue *queue, int numToFetch);
	void wake_queue(Queue *queue);
	void toggle_queue(Queue *queue);
	Queue* queue_init();
	void queue_destroy(Queue *queue);
	Queue *head;
};

#endif /* CONCURRENTQUEUE_H_ */
