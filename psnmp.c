
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

void _buffered_result_and_stats(
	target_t *_current_local,
	int thread_id,
	struct session *_host_ss,
	int status,
	struct snmp_session *sp,
	struct snmp_pdu *response
) {
	
	// NOTE: update stats and see what's next...
	
	char buf[_BUFF_SIZE];
	char _log_str[_BUFF_SIZE];
	
	long _ts1_millis = (_current_local -> _ts1_tv_sec * 1000) + (_current_local -> _ts1_tv_usec / 1000);
	long _ts2_millis = (_current_local -> _ts2_tv_sec * 1000) + (_current_local -> _ts2_tv_usec / 1000);
	
	struct variable_list *vp;
	int ix;
	
	int _duration_millis = _ts2_millis - _ts1_millis;
	
	if (status == STAT_DESCRIP_ERROR) {
	
		// SNMP session error this is...
		// nothing to log. already logged.
		stats.errors++;
		
	} else if (status == STAT_TIMEOUT) {
		
		// TIMEOUT this is...
		stats.no_resp++;
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
		
	} else if (status != STAT_SUCCESS) {
		
		// SNMP error this is...
		stats.errors++;
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
		
	} else if (status == STAT_SUCCESS && response -> errstat != SNMP_ERR_NOERROR) {
		
		// SNMP error this is...
		stats.errors++;
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
		
		vp = response -> variables;
			
		while (vp) {
			
			snprint_variable(buf, sizeof(buf), vp -> name, vp -> name_length, vp);
			sprintf(
				_log_str,
			    "[%8s] tid:%4d, status:%3d, millis: %u, host+oid: %s+%s, result: %s",
			    "answer",
				thread_id,
				STAT_SUCCESS,
				_duration_millis,
				sp -> peername,
				_host_ss -> _oid_name,
				buf
			);
			log2me(DEVELOP, _log_str);
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
		_buffered_result_and_stats(_current_local, worker -> index, _host_ss, _status, &_sess, response);
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
