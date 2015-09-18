/*
 * ConcurrentQueue.cpp
 *
 *  Created on: Sep 17, 2015
 *      Author: michael
 */

#include "ConcurrentQueue.h"

using namespace std;

ConcurrentQueue::ConcurrentQueue() {
	// TODO Auto-generated constructor stub
	head = queue_init();
	cout << "Created new queue" << endl;
}

ConcurrentQueue::~ConcurrentQueue() {
	// TODO Auto-generated destructor stub
	queue_destroy(head);
}

Queue* ConcurrentQueue::queue_init(){
	Queue *queue = NULL;
	queue = (Queue*)malloc(sizeof(Queue));
	if(!queue){
		printf("could not allocate queue\n");
		exit(-1);
	}
	queue->lock = NULL;
	queue->notFull = NULL;
	queue->notEmpty = NULL;

	queue->lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	queue->notFull = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	queue->notEmpty = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));

	if(!queue->lock || !queue->notFull || !queue->notEmpty){
		printf("could not allocate queue lock objects\n");
		exit(-1);
	}

	pthread_mutex_init(queue->lock, NULL);
	pthread_cond_init(queue->notEmpty, NULL);
	pthread_cond_init(queue->notFull, NULL);

	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->keepAlive = 1;

	return queue;
}

void ConcurrentQueue::queue_destroy(Queue *queue){
	QueueItem *temp;
	pthread_mutex_lock(queue->lock);
	queue->count = 0;
	queue->keepAlive = 0;
	while(queue->head){
		temp = queue->head;
		queue->head = queue->head->next;
		free(temp);

	}

	pthread_mutex_unlock(queue->lock);
	pthread_mutex_destroy(queue->lock);
	pthread_cond_destroy(queue->notEmpty);
	pthread_cond_destroy(queue->notFull);

	free(queue);
}

void ConcurrentQueue::enq(QueueItem* start, QueueItem* end, int newItems, Queue *queue){
	/*QueueItem *item = NULL;
	item = malloc(sizeof(QueueItem));

	if(!item){
		printf("could not allocate queue item\n");
		exit(-1);
	}

	item->schema = schema;
	item->next = NULL;
	*/

	pthread_mutex_lock(queue->lock);
	//printf("enqueuing %d items in method with starting count %d\n", newItems, queue->count);
	if(queue->tail){
		queue->tail->next = start;
	}
	queue->tail = end;

	if(!queue->head){
		queue->head = start;
	}

	queue->count += newItems;

	//printf("ending count %d with head %p start %p tail %p end %p\n", queue->count, queue->head, start, queue->tail, end);
	pthread_cond_signal(queue->notEmpty);
	pthread_mutex_unlock(queue->lock);

}

//postcondition: calling threads must free the returned pointer
//QueueItem* ConcurrentQueue::deq(Queue *queue, int *wait, int id, int numToFetch){
QueueItem* ConcurrentQueue::deq(Queue *queue, int numToFetch){
	QueueItem *item;
	QueueItem *last;

	if(!queue->keepAlive){
		return NULL;
	}
	int i;
	pthread_mutex_lock(queue->lock);
	while(queue->keepAlive && queue->count <= 0){
		//printf("waiting with flag: %d and count: %d, system id: %d\n", *wait, queue->count, id);
		cout << "Waiting in deq" <<endl;
		pthread_cond_wait(queue->notEmpty, queue->lock);
		cout << "Waiting finished in deq" << endl;
	}

	if(!queue->keepAlive){
		//printf("exited with id %d\n", id);

		item = NULL;
	} else{

		item = queue->head;
		last = item;
		for(i = 0; queue->count>0 && i<numToFetch; i++){
			queue->head = queue->head->next;

			queue->count--;

		}
		while(last->next != queue->head){
			last = last->next;
		}
		last->next = NULL;

		//printf("count: %d, flag: %d thread id: %d numToFetch: %d dequeued: %d and head: %p\n", queue->count, *wait, id, numToFetch, i, queue->head);

		if(queue->count == 0){
			queue->tail = NULL;
		}
	}

	pthread_mutex_unlock(queue->lock);

	return item;
}

int ConcurrentQueue::getSize(){
	return head->count;
}

void ConcurrentQueue::push(string data, bool keep_alive){

	QueueItem *new_item;
	new_item = new QueueItem;//(QueueItem*)malloc(sizeof(QueueItem));

	new_item->request = data;
	cout << "In push method" << endl;
	new_item->keep_alive = keep_alive;

	enq(new_item, new_item, 1, head);
	cout << "Done with push method. Pushed: " << data << " in struct: " << new_item->request << endl;
}

QueueItem* ConcurrentQueue::pop(){
	return deq(head, 1);
//	if(item == NULL){
//		return "";
//	}
//	string data = item->request;
//	cout << "Popped request: " << data <<endl;
////	free(item);
//	return data;
}

void ConcurrentQueue::wakeQueue(){
	wake_queue(head);
	cout << "Waking queue" <<endl;
}

//wake all waiting threads to check queue
//called when the answer is found or need to kill threads
void ConcurrentQueue::wake_queue(Queue *queue){
	pthread_mutex_lock(queue->lock);
	pthread_cond_broadcast(queue->notEmpty);
	pthread_mutex_unlock(queue->lock);
	//printf("woke queue\n");
}

void ConcurrentQueue::toggleQueue(){
	toggle_queue(head);
	cout << "Toggling queue" <<endl;
}

void ConcurrentQueue::toggle_queue(Queue *queue){
	pthread_mutex_lock(queue->lock);
	queue->keepAlive = 0;
	pthread_cond_broadcast(queue->notEmpty);
	pthread_mutex_unlock(queue->lock);
	//printf("toggled queue\n");
}
