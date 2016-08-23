
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
	
	// 'll put my local log messages here;
	char _log_str[_BUFF_SIZE];
	
	// ts based benchmarking;
	struct timeval _now;
	
	log_step_message(LOW, "prlogger is going to START.");
	
	if ( set.db_off == FALSE ) {
		
		log_step_message(LOW, "prlogger is going to check DATABASE.");
		
		if (_db_connect(set._db_name, &_mysql) < 0) {
			
			log_step_message(LOW, "prlogger database check FAILED. Please verify your configuration.");
			
			// NOTE: shutting down db inserts;
			set.db_off = TRUE;
			
		} else {
			
			if (mysql_ping(&_mysql) == 0) {
				
				log_step_message(LOW, "prlogger database check is OK.");
				_stats -> logger_db_alive = TRUE;
				
			} else {
				
				log_step_message(LOW, "prlogger database check FAILED. Server is not responding");
				
				// NOTE: shutting down db inserts;
				set.db_off = TRUE;
				
			}
			
		}
		
	}

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
			
			if (set.db_off == FALSE) {
				
				// NOTE: 'll generate query and execute it;
				
				char query[_BUFF_SIZE];
				snprintf(
					query,
					sizeof(query),
					"INSERT INTO `%s` VALUES (%d, FROM_UNIXTIME(%ld), %llu)",
					_entry -> table,
					_entry -> iid,
					_entry -> _ts2_tv_sec,
					_entry -> insert_value
				);

//				stats.db_inserts++;
				int _dbq_status = mysql_query(&_mysql, query);
				
				if ( _dbq_status != 0) {
					
					// FIXME: only through dev phase!!! Do not need this in prod;
					if (mysql_errno(&_mysql) == 1146) {
						
						 _db_create_counter_store_rel(&_mysql, _entry -> table, query);
						
					} else {
						
						sprintf(
							_log_str,
							"[%8s] database no: %d,  decr: %s",
							"error",
							mysql_errno(&_mysql),
							mysql_error(&_mysql)
						);
						
						log2me(DEBUG, _log_str);
						
					}
					
				};

//	if (!(set.dboff)) {
//		if ( (insert_val > 0) || (set.withzeros) ) {
//			PT_MUTEX_LOCK(&crew->mutex);
//			snprintf(query, sizeof(query), "INSERT INTO %s VALUES (%d, NOW(), %llu)",
//			         entry->table, entry->iid, insert_val);
//			if (set.verbose >= DEBUG) printf("SQL: %s\n", query);
//			status = mysql_query(&mysql, query);
//			if (status) printf("*** MySQL Error: %s\n", mysql_error(&mysql));
//			PT_MUTEX_UNLOCK(&crew->mutex);
//
//			if (!status) {
//				PT_MUTEX_LOCK(&stats.mutex);
//				stats.db_inserts++;
//				PT_MUTEX_UNLOCK(&stats.mutex);
//			}
//		}
//		insert_val > 0 or withzeros
//	}
				
				
			}
			
			
			free(_entry);
		}
		
		gettimeofday(&_now, NULL);
		
		PT_MUTEX_UNLOCK( &(_stats -> mutex) );
		
	}
	
}
