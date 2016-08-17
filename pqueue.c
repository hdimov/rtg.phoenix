
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

target_t* _target_dup(target_t* _src) {
	
	if (_src == NULL) return NULL;
	
	target_t* _node = malloc(sizeof(target_t));
	
	// NOTE: lean && mean node initialization;
	memset(_node, 0, sizeof(target_t));
	
/*

	char host[64];
	char objoid[128];

	// NOTE: bits are set to 0, we interpret it as gauge values;
	unsigned short bits;

	char community[64];
	char table[64];

	unsigned int iid;

	char iface[64];

	// NOTE: why this is signed?
	long long maxspeed;
	
	enum targetState init;
	
	unsigned long long last_value;
	int last_status_sess;
	int last_status_snmp;
	
	// NOTE: adding this for future use;
	unsigned long long prev_value;
	
	// also make necessary changes in order to log RTT time of last target(s) poll;

	// system time the request was sent;
	long _ts1_tv_sec;
	int _ts1_tv_usec;

	// system time the answer was received;
	long _ts2_tv_sec;
	int _ts2_tv_usec;

	struct target_struct *next;
 
 */

	// NOTE: real data copy;
	
	// char host[64];
	memcpy(_node -> host, _src -> host, 64);
	// char objoid[128];
	memcpy(_node -> objoid, _src -> objoid, 128);
	
	// unsigned short bits;
	_node -> bits = _src -> bits;
	
	// char community[64];
	memcpy(_node -> community, _src -> community, 64);
	// char table[64];
	memcpy(_node -> table, _src -> table, 64);
	
	// unsigned int iid;
	_node -> iid = _src -> iid;
	
	// char iface[64];
	memcpy(_node -> iface, _src -> iface, 64);
	
	// long long maxspeed;
	_node -> maxspeed = _src -> maxspeed;
	
	// enum targetState init;
	_node -> init = _src -> init;
	
	// unsigned long long last_value;
	_node -> last_value = _src -> last_value;
	
	// int last_status_sess;
	_node -> last_status_sess = _src -> last_status_sess;
	// int last_status_snmp;
	_node -> last_status_snmp = _src -> last_status_snmp;
	
	// unsigned long long prev_value;
	_node -> prev_value = _src -> prev_value;
	
	// system time the request was sent;
	
	// long _ts1_tv_sec;
	_node -> _ts1_tv_sec = _src -> _ts1_tv_sec;
	// int _ts1_tv_usec;
	_node -> _ts1_tv_usec = _src -> _ts1_tv_usec;
	
	// system time the answer was received;
	
	// long _ts2_tv_sec;
	_node -> _ts2_tv_sec = _src -> _ts2_tv_sec;
	// int _ts2_tv_usec;
	_node -> _ts2_tv_usec = _src -> _ts2_tv_usec;
	
	// struct target_struct *next;
	// NOTE: or make real copy?!
	_node -> next = NULL;
	
	return _node;

}