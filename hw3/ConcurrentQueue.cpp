/*
 * ConcurrentQueue.cpp
 *
 *  Author: Michael Coughlin
 *  ECEN 5023: Network Systems Hw 1
 *  Sept. 20, 2015
 */

#include "ConcurrentQueue.h"

using namespace std;

ConcurrentQueue::ConcurrentQueue() {
	head = queue_init();
}

ConcurrentQueue::~ConcurrentQueue() {
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

	queue->keepAlive = 0;
	while(queue->head){
		temp = queue->head;
		queue->head = queue->head->next;
		free(temp);
	}

	queue->count = 0;

	pthread_mutex_unlock(queue->lock);
	pthread_mutex_destroy(queue->lock);
	pthread_cond_destroy(queue->notEmpty);
	pthread_cond_destroy(queue->notFull);

	free(queue);
}

void ConcurrentQueue::enq(QueueItem* start, QueueItem* end, int newItems, Queue *queue){
	pthread_mutex_lock(queue->lock);
	if(queue->tail){
		queue->tail->next = start;
	}
	queue->tail = end;

	if(!queue->head){
		queue->head = start;
	}

	queue->count += newItems;

	pthread_cond_signal(queue->notEmpty);
	pthread_mutex_unlock(queue->lock);

}

//postcondition: calling threads must free the returned pointer
//QueueItem* ConcurrentQueue::deq(Queue *queue, int *wait, int id, int numToFetch){
QueueItem* ConcurrentQueue::deq(Queue *queue, int numToFetch){
	QueueItem *item;
	QueueItem *last;

	int i;
	pthread_mutex_lock(queue->lock);
	while(queue->keepAlive && queue->count <= 0){
		pthread_cond_wait(queue->notEmpty, queue->lock);
	}
	if(numToFetch > queue->count){
		return NULL;
	}
	if(!queue->keepAlive){
		return NULL;
	}

	if(!queue->keepAlive){
		item = NULL;
	} else{

		item = queue->head;
		last = item;
		for(i = 0; queue->count>0 && i<numToFetch; i++){
			if(queue -> head != NULL){
				queue->head = queue->head->next;

				queue->count--;
			} else{
				break;
			}
		}
		while(last->next != queue->head){
			last = last->next;
		}
		last->next = NULL;

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

void ConcurrentQueue::push(string data){
	void* raw = malloc(sizeof(QueueItem));
	QueueItem *new_item;
	new_item = new(raw) QueueItem;

	new_item->request = data;

	//IMPORTANT: need to set next to NULL or there may be a seg fault
	new_item->next = NULL;

	enq(new_item, new_item, 1, head);
//	wakeQueue();
}

QueueItem* ConcurrentQueue::pop(){
//	cout << "Attempting to pop" <<endl;
	return deq(head, 1);
}

void ConcurrentQueue::wakeQueue(){
	wake_queue(head);
}

//wake all waiting threads to check queue
//called when the answer is found or need to kill threads
void ConcurrentQueue::wake_queue(Queue *queue){
	pthread_mutex_lock(queue->lock);
	pthread_cond_broadcast(queue->notEmpty);
	pthread_mutex_unlock(queue->lock);
}

void ConcurrentQueue::toggleQueue(){
	toggle_queue(head);
}

void ConcurrentQueue::toggle_queue(Queue *queue){
	pthread_mutex_lock(queue->lock);
	queue->keepAlive = 0;
	pthread_cond_broadcast(queue->notEmpty);
	pthread_mutex_unlock(queue->lock);
}
