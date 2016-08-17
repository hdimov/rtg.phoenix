
/****************************************************************************
   Program:     pqueue.c, v0.7.4-r1
   Author(s):   hdimov
   Purpose:     RTG Phoenix Queue data structure tools;
****************************************************************************/

//
// Created by Hristo Hristov on 7/9/16.
//

#include "rtg.h"

queue_t* _q_alloc() {
	
	queue_t* q = (queue_t *) malloc( sizeof(queue_t) );
	
	// NOTE: malloc() err condtion check;
	
	q -> _elem_count = 0;
	q -> _size = 0;
	
	q -> _elem_list = NULL;
	q -> _elem_last = NULL;

	return q;
}

void _q_free(queue_t* q, int _free_data) {
	
	if (q == NULL)
		return;
	
}

int _q_exists(queue_t* q) {

	return q != NULL;

}

int _q_is_empty(queue_t* q) {
	
	if (_q_exists(q) && q -> _elem_count > 0)
		return 0;

	return 1;
	
}

int _q_push(queue_t* q, void* src, int len) {
	
	// create queue element;
	queue_node_t* _node = ( queue_node_t* ) malloc(sizeof(queue_node_t));
	// NOTE: possible error condition;
	
	_node -> _data = src;
	_node -> _size = len;
	_node -> _next = NULL;
	
	if (q -> _elem_count == 0) {
		
		q -> _elem_list = q -> _elem_last = _node;
		
		q -> _elem_count = 1;
		q -> _size = len;
		
		return 1;
		
	}
	
	q -> _elem_last -> _next = _node;
	q -> _elem_last = _node;

	q -> _size += len;
	q -> _elem_count += 1;
	
	return 1;
	
}

void* _q_pop(queue_t* q) {
	
	if (!_q_exists(q))
		return NULL;

	if (_q_is_empty(q))
		return NULL;
	
	queue_node_t* _node = q -> _elem_list;
	void* _data = _node -> _data;
	
	q -> _elem_list = _node -> _next;
	
	q -> _elem_count -= 1;
	q -> _size -= _node -> _size;
	
	free(_node);
	return _data;
	
}
