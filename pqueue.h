
/****************************************************************************
   Program:     pqueue.c, v0.7.4-r1
   Author(s):   hdimov
   Purpose:     RTG Phoenix Queue data structure tools;
****************************************************************************/

//
// Created by Hristo Hristov on 7/9/16.
//

#ifndef RTG_PHOENIX_PQUEUE_H
#define RTG_PHOENIX_PQUEUE_H

typedef struct queue_node_struct {

	void* data;
	struct queue_node_struct *next;

} queue_node_t;

typedef struct queue_struct {

	int _elem_count;
	queue_node_t *_elem_list;

} queue_t;

#endif //RTG_PHOENIX_PQUEUE_H

// _q_alloc
// _q_free
//
// _q_is_empty
//
// _q_enq/_q_push
// _q_deq/_q_pop
//
// _q_clone

