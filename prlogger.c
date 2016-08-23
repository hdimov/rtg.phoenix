
/****************************************************************************
   Program:     pqueue.c, v0.7.4-r2
   Author(s):   hdimov
   Purpose:     RTG Phoenix Result Logger;
****************************************************************************/

//
// Created by Hristo Hristov on 8/22/16.
//

#include "prlogger.h"

void* prlogger(void *_thread_args) {

	stats_t *_stats = (stats_t *) _thread_args;
	
	// ts based benchmarking;
	struct timeval _now;
	
	// 'll put my log messages here;
	char _log_str[_BUFF_SIZE];
	
	// NOTE: connect to db, right here. Disconnect is not mandatory at the moment.
	
	/* Attempt to connect to the MySQL Database */
	// #ifndef _DB_BACKEND_DISABLED
	if (!(set.dboff)) {
		if (_db_connect(set.dbdb, &mysql) < 0) {
			fprintf(stderr, "** Database error - check configuration.\n");
			exit(-1);
		}
		if (!mysql_ping(&mysql)) {
			if (set.verbose >= LOW)
				printf("connected.\n");
		} else {
			printf("server not responding.\n");
			exit(-1);
		}
	}
	// #endif

	for ( ; ; ) {
		
		PT_MUTEX_LOCK( &(_stats -> mutex) );
		
		log_step_message(LOW, "prlogger is going to SLEEP.");
		
		PT_COND_WAIT(&(_stats -> go), &(_stats -> mutex));
		// usleep(5 * 1000 * 1000); // 100 millis
		gettimeofday(&_now, NULL);
		
		log_step_message(LOW, "prlogger is going to WORK.");
		
		while (!_q_is_empty(_stats -> _q_result)) {
			
			target_t* _entry = _q_pop(_stats -> _q_result);
			
			sprintf(
				_log_str,
				"[%8s] host+oid: %s+%s, result: %llu",
				"prlog",
				_entry -> host,
				_entry -> objoid,
			    _entry -> insert_value
			);
			log2me(DEBUG, _log_str);
			
			free(_entry);
		}
		
		gettimeofday(&_now, NULL);
		
		PT_MUTEX_UNLOCK( &(_stats -> mutex) );
		
	}
	
}
