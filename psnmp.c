
/****************************************************************************
   Program:     psnmp.c, v0.7.4-r2
   Author(s):   hdimov
   Purpose:     RTG Phoenix SNMP infrastructure; Main poller engine.
****************************************************************************/

// Created by Hristo Hristov on 7/9/16.

#include "psnmp.h"

// global pointer to our OID hash and single poll result counters;
extern target_t *current;
extern stats_t stats;

// 'll help me to ... poll all hosts in parallel ;)

struct session {

	/* SNMP session data */
	struct snmp_session *_sess;

	/* how far in our poll are we? */
	const char *_oid_name;

	oid _oid[MAX_OID_LEN];
	int _oid_len;

};

void _anylyze_result_and_update_queue(
	target_t *_current_local,
	int thread_id,
	struct session *_host_ss,
	int status,
	struct snmp_session *sp,
	struct snmp_pdu *response,
	struct variable_list* var
) {
	
	// char buf[_BUFF_SIZE];
	char _log_str[_BUFF_SIZE];
	
	unsigned long long result = 0;
	unsigned long long insert_val = 0;
	
	if (_current_local -> last_status_sess != STAT_SUCCESS || _current_local -> last_status_snmp != SNMP_ERR_NOERROR) {
		
		_current_local -> prev_value = _current_local -> last_value;
		_current_local -> last_value = result;
		
		insert_val = 0; // upon init log 0 because of possible high edges on the graph;
		_current_local -> insert_value = insert_val;
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins (llu) result: %llu",
			"ir.error",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val
		);
		
		log2me(DEVELOP, _log_str);
		
		return;
	}

	if (var == NULL) return;
	
	// if (_current_local -> last_status_sess == STAT_SUCCESS && _current_local -> last_status_snmp == SNMP_ERR_NOERROR) {
	
	switch (var -> type) {
		
		 // Switch over vars->type and modify/assign result accordingly.
		case ASN_COUNTER64:
			// if (set.verbose >= DEBUG) printf("64-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
			result = (var -> val).counter64 -> high;
			result = result << 32;
			result = result + (var -> val).counter64 -> low;
			break;
			
		case ASN_COUNTER:
			// if (set.verbose >= DEBUG) printf("32-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
			result = (unsigned long) *((var -> val).integer);
			break;
			
		case ASN_INTEGER:
			// if (set.verbose >= DEBUG) printf("Integer result: (%s@%s) %s\n", session.peername, storedoid, result_string);
			result = (unsigned long) *((var -> val).integer);
			break;
			
		case ASN_GAUGE:
			// if (set.verbose >= DEBUG) printf("32-bit gauge: (%s@%s) %s\n", session.peername, storedoid, result_string);
			result = (unsigned long) *((var -> val).integer);
			break;
			
		// about timeticks type: http://stackoverflow.com/questions/14572006/net-snmp-returned-types
		case ASN_TIMETICKS:
			// if (set.verbose >= DEBUG) printf("Timeticks result: (%s@%s) %s\n", session.peername, storedoid, result_string);
			result = (unsigned long) *((var -> val).integer);
			break;
/*

		https://www.mail-archive.com/net-snmp-users@lists.sourceforge.net/msg16519.html
		
		case ASN_OPAQUE:
			// if (set.verbose >= DEBUG) printf("Opaque result: (%s@%s) %s\n", session.peername, storedoid, result_string);
			result = (unsigned long) *(var -> val.integer);
			break;

 */
		default:
			// if (set.verbose >= DEBUG) printf("Unknown result type: (%s@%s) %s\n", session.peername, storedoid, result_string);
			// FIXME: but warn for it;
			result = 0;
			;
	}
	
	// gauge type we have;
	if (_current_local -> bits == 0) {
		
		//FIXME: change detection matters here;
		_current_local -> prev_value = _current_local -> last_value;
		_current_local -> last_value = result;

		// if (_current_local -> last_value != _current_local -> prev_value) {
			// we have change, so log it;
		// }

		insert_val = result;
		_current_local -> insert_value = insert_val;

//				printf("Thread [%d]: Gauge steady at %lld\n", worker->index, insert_val);
//				printf("Thread [%d]: Gauge change from %lld to %lld\n", worker->index, last_value, insert_val);
//      whith zeroes in interpreted that way here;
//			if (set.withzeros)
//				insert_val = result;
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins (llu) result: %llu",
			"ir.gauge",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val
		);
		
	// NOTE: counter wrap condition || device (interface) restart condition;
	} else if (result < _current_local -> last_value) {
		
		stats.wraps++;
		
		_current_local -> prev_value = _current_local -> last_value;
		_current_local -> last_value = result;
		
		if (_current_local -> bits == 32)
			insert_val = (_THIRTY_TWO - _current_local -> prev_value) + result;
		else if (_current_local -> bits == 64)
			insert_val = (_SIXTY_FOUR - _current_local -> prev_value) + result;
		
//		if (insert_val > _current_local -> maxspeed) {
//			insert_val = 0;
//		}
		
		_current_local -> insert_value = insert_val;

//		PT_MUTEX_LOCK(&stats.mutex);
//		stats.wraps++;
//		PT_MUTEX_UNLOCK(&stats.mutex);

//		if (set.verbose >= LOW) {
//			printf("*** Counter Wrap (%s@%s) [poll: %llu][last: %llu][insert: %llu]\n",
//			       session.peername, storedoid, result, last_value, insert_val);
//		}
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins (llu) result: %llu",
			"ir.wrap",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val
		);

	// Not a counter wrap and this is not the first poll
	} else if ( (_current_local -> last_value >= 0) && (_current_local -> init != NEW)) {
		
		_current_local -> prev_value = _current_local -> last_value;
		_current_local -> last_value = result;
		
		insert_val = result - _current_local -> prev_value;
		
		/* Print out SNMP result if verbose */
//		if (set.verbose == DEBUG)
//			printf("Thread [%d]: (%lld-%lld) = %llu\n", worker->index, result, last_value, insert_val);
//		if (set.verbose == HIGH)
//			printf("Thread [%d]: %llu\n", worker->index, insert_val);
		/* last_value < 0, so this must be the first poll */
		
		_current_local -> insert_value = insert_val;
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins result (octets.llu): %llu, speed result: %.3f Kbits/s, %.3f KBytes/s",
			"ir.live",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val,
			( (insert_val * 8.0 ) / (double) set.interval) / 1000.0,
			( (double) insert_val / (double) set.interval) / 1024.0
		);
		
	} else if ( (_current_local -> last_value >= 0) && (_current_local -> init == NEW)) {
		
		_current_local -> init = LIVE;
		
		_current_local -> prev_value = _current_local -> last_value;
		_current_local -> last_value = result;
		
		insert_val = 0; // upon init log 0 because of possible high edges on the graph;
		_current_local -> insert_value = insert_val;
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins (llu) result: %llu",
			"ir.new",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val
		);
		
	} else {
		
		_current_local -> prev_value = _current_local -> last_value;
		_current_local -> last_value = result;
		
		insert_val = 0; // upon init log 0 because of possible high edges on the graph;
		_current_local -> insert_value = insert_val;
		
//		exit(-1);
//		if (set.verbose >= HIGH) printf("Thread [%d]: First Poll, Normalizing\n", worker->index);
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins (llu) result: %llu",
			"ir.else",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val
		);
		
	}
	
	/* Check for bogus data, either negative or unrealistic */
	if (insert_val > _current_local -> maxspeed || result < 0) {
		
		stats.out_of_range++;
		
		insert_val = 0;
		_current_local -> insert_value = insert_val;
		
//		if (set.verbose >= LOW) printf("*** Out of Range (%s@%s) [insert_val: %llu] [oor: %lld]\n",
//		                               session.peername, storedoid, insert_val, entry->maxspeed);
//		insert_val = 0;
//		PT_MUTEX_LOCK(&stats.mutex);
//		stats.out_of_range++;
//		PT_MUTEX_UNLOCK(&stats.mutex);
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, ins (llu) result: %llu",
			"ir.oor",
			thread_id,
			STAT_SUCCESS,
			sp -> peername,
			_host_ss -> _oid_name,
			insert_val
		);
		
	}
	
	log2me(DEVELOP, _log_str);
	
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

//if (sessp != NULL) {
//snmp_sess_close(sessp);
//if (response != NULL) snmp_free_pdu(response);
//}

//if (set.verbose >= DEVELOP)
//printf("Thread [%d] locking (update work_count)\n", worker->index);
//PT_MUTEX_LOCK(&crew->mutex);
//crew->work_count--;
///* Only if we received a positive result back do we update the
//   last_value object */
//
//if (status == STAT_SUCCESS) entry->last_value = result;
//if (init == NEW) entry->init = LIVE;
//if (crew->work_count <= 0) {
//if (set.verbose >= HIGH) printf("Queue processed. Broadcasting thread done condition.\n");
//PT_COND_BROAD(&crew->done);
//}
//if (set.verbose >= DEVELOP)
//printf("Thread [%d] unlocking (update work_count)\n", worker->index);
//
//PT_MUTEX_UNLOCK(&crew->mutex);

}

void _log_result_and_update_stats(
	target_t* _current_local,
	int thread_id,
	struct session* _host_ss,
	int status,
	struct snmp_session* sp,
	struct snmp_pdu* response
) {
	
	// NOTE: update stats and see what's next...

	char buf[_BUFF_SIZE];
	char _log_str[_BUFF_SIZE];
	
	long _ts1_millis = (_current_local -> _ts1_tv_sec * 1000) + (_current_local -> _ts1_tv_usec / 1000);
	long _ts2_millis = (_current_local -> _ts2_tv_sec * 1000) + (_current_local -> _ts2_tv_usec / 1000);
	
	struct variable_list *vp;
	int ix;
	
	int _duration_millis = _ts2_millis - _ts1_millis;
	
	_current_local -> last_status_sess = status;
	
	if (status == STAT_DESCRIP_ERROR) {
	
		// SNMP session error this is...
		// nothing to log. already logged.
		
		
		stats.errors++;
		_current_local -> last_status_snmp = status;
		
		_anylyze_result_and_update_queue(_current_local, thread_id, _host_ss, status, sp, response, NULL);
		
	} else if (status == STAT_TIMEOUT) {
		
		// TIMEOUT this is...
		stats.no_resp++;
		_current_local -> last_status_snmp = status;
		
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, millis: %u, host+oid: %s+%s, result: %s",
			"timeout",
			thread_id,
			STAT_TIMEOUT,
			_duration_millis,
			sp -> peername,
			_host_ss -> _oid_name,
			"timeout"
		);
		log2me(DEBUG, _log_str);
		
		_anylyze_result_and_update_queue(_current_local, thread_id, _host_ss, status, sp, response, NULL);
		
	} else if (status != STAT_SUCCESS) {
		
		// SNMP error this is...
		stats.errors++;
		_current_local -> last_status_snmp = status;

//		printf("*** SNMP Error: (%s@%s) Unsuccessuful (%d).\n", session.peername,
//		       storedoid, status);
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
			"error",
			thread_id,
			STAT_ERROR,
			sp -> peername,
			_host_ss -> _oid_name,
			"FIXME: status != STAT_SUCCESS"
		);
		log2me(DEBUG, _log_str);
		
		_anylyze_result_and_update_queue(_current_local, thread_id, _host_ss, status, sp, response, NULL);
		
	} else if (status == STAT_SUCCESS && response -> errstat != SNMP_ERR_NOERROR) {
		
		// SNMP error this is...
		stats.errors++;
		_current_local -> last_status_snmp = response -> errstat;
		
//		printf("*** SNMP Error: (%s@%s) %s\n", session.peername, storedoid, snmp_errstring(response->errstat));
		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
			"error",
			thread_id,
			STAT_ERROR,
			sp -> peername,
			_host_ss -> _oid_name,
			"FIXME: status == STAT_SUCCESS && response -> errstat != SNMP_ERR_NOERROR"
		);
		log2me(DEBUG, _log_str);
		
		_anylyze_result_and_update_queue(_current_local, thread_id, _host_ss, status, sp, response, NULL);

// FIXME:
//
//	} else {
//
//		stats.errors++;
//
//		for (ix = 1; vp && ix != response->errindex; vp = vp->next_variable, ix++);
//
//		if (vp)
//			snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
//		else
//			strcpy(buf, "(none)");
//
//		ts2(buf);
//
//		fprintf(stdout, ":%s: %s: %s\n",
//		        sp->peername, buf, snmp_errstring(pdu->errstat));
//
//	}
		
	} else if (status == STAT_SUCCESS && response -> errstat == SNMP_ERR_NOERROR) {
		
		// OK this is ...
		stats.polls++;
		_current_local -> last_status_snmp = response -> errstat;
		
		vp = response -> variables;
			
		while (vp) {
			
			snprint_variable(buf, sizeof(buf), vp -> name, vp -> name_length, vp);
			
			// sprint_value(result_string, anOID, anOID_len, vars);
			// snprint_value(result_string, BUFSIZE, anOID, anOID_len, vars);
			
			sprintf(
				_log_str,
			    "[%8s] tid:%4d, status:%3d, millis: %u, host+oid: %s+%s, text result: %s",
			    "answer",
				thread_id,
				STAT_SUCCESS,
				_duration_millis,
				sp -> peername,
				_host_ss -> _oid_name,
				buf
			);
			log2me(DEVELOP, _log_str);
			
			_anylyze_result_and_update_queue(_current_local, thread_id, _host_ss, status, sp, response, vp);
			
			vp = vp -> next_variable;
			
		}
		
	}
	
}

// better error handling and result understanding -> v2 of sync_poller;
void* sync_poller_v2(void *thread_args) {

	// who am I?
	worker_t* worker = (worker_t *) thread_args;
	// where are the others?
	crew_t* crew = worker -> crew;
	
	// what is my current target?
	target_t *_current_local = NULL;
	
	// ts based benchmarking;
	struct timeval _now;
	
	// 'll put my log messages here;
	char _log_str[_BUFF_SIZE];
	
	// 'll wait until all other threads are ready;
	PT_MUTEX_LOCK(&crew -> mutex);
	PT_COND_WAIT(&crew -> go, &crew -> mutex);
	PT_MUTEX_UNLOCK(&crew -> mutex);
	
	for( ; ; ) {

		// NOTE: critical section I
		// get my next target;
		// also check if there is more work to do;
		
		PT_MUTEX_LOCK(&crew -> mutex);
		
		current = _current_local = getNext();
		crew -> _send_work_count--;
		
		if (_current_local == NULL && crew -> _send_work_count <= 0) {
			
			crew -> _send_worker_count--;
			PT_COND_BROAD(&crew -> _sending_done);
			
			PT_COND_WAIT(&crew -> go, &crew -> mutex);
			PT_MUTEX_UNLOCK(&crew -> mutex);
			continue;
		}
		
		PT_MUTEX_UNLOCK(&crew->mutex);
		
		// work received, lets start benchmarking;
		
		// have _current_local may init ts1/ts2 ...
		gettimeofday(&_now, NULL);
		_current_local -> _ts1_tv_sec = _now.tv_sec;
		_current_local -> _ts1_tv_usec = _now.tv_usec;
		
		// let's make a SNMP session;
		
		// struct session *_host_ss = stuepd you are, this is a ptr! should be initialized 1st :- ;
		struct session *_host_ss = calloc(1, sizeof(struct session));
		
		struct snmp_session _sess;
		
		// this will show us snmp_session status;
		int _status;
		
		struct snmp_pdu *pdu = NULL;
		struct snmp_pdu *response = NULL;
		
		// initialize snmp session struct;
		snmp_sess_init(&_sess);
		
		_sess.version = SNMP_VERSION_2c;
		
		// NOTE: be aware on free();
		_sess.peername = strdup(_current_local-> host);
		_sess.community = strdup(_current_local -> community);
		
		_sess.community_len = strlen(_sess.community);
		
		_host_ss -> _sess = snmp_sess_open(&_sess);

		if (_host_ss -> _sess == NULL) {
			
			_status = STAT_DESCRIP_ERROR;
			
			sprintf(
				_log_str,
				"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
				"answer",
				worker -> index,
				STAT_DESCRIP_ERROR,
				_current_local -> host,
				_current_local -> objoid,
				"snmp descriptor error"
			);
			log2me(DEVELOP, _log_str);
			
		} else {
			
			// NOTE: be aware on free();
			_host_ss -> _oid_name = strdup(_current_local -> objoid);
			_host_ss -> _oid_len = sizeof(_host_ss -> _oid) / sizeof(_host_ss -> _oid[0]);
			
			if (!read_objid(_host_ss -> _oid_name, _host_ss -> _oid, (size_t*) (&_host_ss -> _oid_len))) {
				
				// FIXME: still to be handled nicely when happends...
				snmp_perror("TERRIBLE error with read_objid() call...");
				exit(-1);
				
			}
			
			// create snmp GET
			pdu = snmp_pdu_create(SNMP_MSG_GET);
			snmp_add_null_var(pdu, _host_ss -> _oid, _host_ss -> _oid_len);
			
			sprintf(
				_log_str,
				"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
				"query",
				worker -> index,
				STAT_SUCCESS,
				_current_local -> host,
				_current_local -> objoid,
				"sending request..."
			);
			log2me(DEVELOP, _log_str);
			
			_status = snmp_sess_synch_response(_host_ss -> _sess, pdu, &response);
			
		}
		
		gettimeofday(&_now, NULL);
		_current_local -> _ts2_tv_sec = _now.tv_sec;
		_current_local -> _ts2_tv_usec = _now.tv_usec;
		
		PT_MUTEX_LOCK(&stats.mutex);
		_log_result_and_update_stats(_current_local, worker -> index, _host_ss, _status, &_sess, response);
		// NOTE: got proper result for _current_result let's add it to queue;
		_q_push(stats._q_result, _target_dup(_current_local), sizeof(target_t));
		PT_MUTEX_UNLOCK(&stats.mutex);
		
		// allways free...
		// _sess.peername = strdup(_current_local-> host);
		// _sess.community = strdup(_current_local -> community);
		free(_sess.peername);
		free(_sess.community);
		
		if (_status != STAT_DESCRIP_ERROR) {
			
			// _host_ss -> _oid_name = strdup(_current_local -> objoid);
			free((void*) _host_ss -> _oid_name);
			
			snmp_sess_close(_host_ss -> _sess);
			snmp_free_pdu(response);
			
		}
		
		// allways free...
		free(_host_ss);
		
	}
	
}
