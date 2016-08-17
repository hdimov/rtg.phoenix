
/****************************************************************************
   Program:     pqueue.c, v0.7.4-r1
   Author(s):   hdimov
   Purpose:     RTG Phoenix Queue data structure tools;
****************************************************************************/

//
// Created by Hristo Hristov on 7/9/16.
//

#ifndef RTG_PHOENIX_PQUEUE_H
#define RTG_PHOENIX_PQUEUE_H 1

typedef struct queue_node_struct {

	void* _data;
	unsigned int _size; // in order to allow _q_clone;

	struct queue_node_struct* _next;

} queue_node_t;

typedef struct queue_struct {

	int _elem_count;
	int _size; // total queue size; just in case;
	
	queue_node_t* _elem_list;
	queue_node_t* _elem_last;

} queue_t;

// queue elements will be target_t elemens;

// _q_alloc
// _q_free

// _q_is_empty

// _q_enq/_q_push
// _q_deq/_q_pop

// ?! _q_clone

queue_t* _q_alloc();
void _q_free(queue_t *q, int _free_data);

int _q_exists(queue_t *q);
int _q_is_empty(queue_t *q);

int _q_push(queue_t* q, void* src, int len);
void* _q_pop(queue_t* q);

#endif //RTG_PHOENIX_PQUEUE_H

