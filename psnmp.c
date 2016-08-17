
/****************************************************************************
   Program:     psnmp.c, v0.7.4-r1
   Author(s):   hdimov
   Purpose:     RTG Phoenix SNMP infrastructure;
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


int _buffered_print_result(
	target_t *_current_local,
	int thread_id,
	struct session *_host_ss,
	int status,
	struct snmp_session *sp,
	struct snmp_pdu *response
) {

	char buf[_BUFF_SIZE];
	char _log_str[_BUFF_SIZE];

	struct variable_list *vp;
	int ix;

//	fprintf(stdout, "%.2d:%.2d:%.2d.%.6d, status: %d, ", tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec, status);

	long _ts1_millis = (_current_local -> _ts1_tv_sec * 1000) + (_current_local -> _ts1_tv_usec / 1000);
	long _ts2_millis = (_current_local -> _ts2_tv_sec * 1000) + (_current_local -> _ts2_tv_usec / 1000);

//	long _tv_sec_duration = _current_local -> _ts2_tv_sec - _current_local -> _ts1_tv_sec;
//	int _tv_usec_duration = 1000000 - _current_local->_ts1_tv_usec + _current_local -> _ts2_tv_usec;
//
//	double _duration_millis = (_tv_sec_duration * 10000000 + _tv_usec_duration) / 1000000 * 1000;

	int _duration_millis = _ts2_millis - _ts1_millis;

	switch (status) {

		case STAT_SUCCESS:

			vp = response -> variables;
			if (response -> errstat == SNMP_ERR_NOERROR) {

				while (vp) {
					snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
					sprintf(
						_log_str,
						// "[%8s] tid:%4d, status:%3d, ts1_ts2: %lu.%u_%lu.%u, host+oid: %s+%s, result: %s",
						"[%8s] tid:%4d, status:%3d, millis: %u, host+oid: %s+%s, result: %s",
						"info",
						thread_id,
						STAT_SUCCESS,
						_duration_millis,
						sp -> peername,
						_host_ss -> _oid_name,
						buf
					);
					log2me(DEVELOP, _log_str);
					vp = vp->next_variable;
				}

			} else {

				for (ix = 1; vp && ix != response->errindex; vp = vp->next_variable, ix++);

				if (vp)
					snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
				else
					strcpy(buf, "(none)");

				ts2(buf);

//				fprintf(stdout, ":%s: %s: %s\n",
//				        sp->peername, buf, snmp_errstring(pdu->errstat));

			}

			return 1;

		case STAT_TIMEOUT:
			sprintf(
				_log_str,
				//"[%8s] tid:%4d, status:%3d, ts1_ts2: %lu.%u_%lu.%u, host+oid: %s+%s, result: %s",
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

			// ts2(_log_str);
			// fprintf(stdout, ":%s: Timeout\n", sp -> peername);
			return 0;


		case STAT_ERROR:

			// NOTE: this one indicated s snmp lib error;

			sprintf(
				_log_str,
				"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
				"error",
				thread_id,
				STAT_ERROR,
				sp -> peername,
				_host_ss -> _oid_name,
			    "FIXME"
			);

			log2me(DEBUG, _log_str);
			// ts2(_log_str);
			// snmp_perror(sp->peername);
			return 0;

	}

	return 0;

}

// phoenix sync poller; thread overloaded (r1) version;
void *sync_poller(void *thread_args) {

	// who am I?
	worker_t *worker = (worker_t *) thread_args;
	crew_t *crew = worker -> crew;

	// what is my current target?
	target_t *_current_local = NULL;

	struct timeval _now;

	char _log_str[_BUFF_SIZE];

	PT_MUTEX_LOCK(&crew -> mutex);
	PT_COND_WAIT(&crew -> go, &crew -> mutex);
	PT_MUTEX_UNLOCK(&crew -> mutex);

	for( ; ; ) {

		// preparing snmp session;
		// we got what we need from current entry
		// moving to next entry for other waiting threads/workers;

		// critical work loading section;
		PT_MUTEX_LOCK(&crew -> mutex);
		current = _current_local = getNext();
		crew -> _send_work_count--;
		if (_current_local == NULL && crew->_send_work_count <= 0) {
			crew -> _send_worker_count--;
			PT_COND_BROAD(&crew -> _sending_done);
			PT_COND_WAIT(&crew -> go, &crew -> mutex);
			PT_MUTEX_UNLOCK(&crew -> mutex);
			continue;
		}
		PT_MUTEX_UNLOCK(&crew -> mutex);

		// NOTE: lets make a snmp session;

		// struct session *_host_ss = stuepd you are, this is a ptr! sohuld be initialized 1st :- ;
		struct session *_host_ss = calloc(1, sizeof(struct session));

		struct snmp_session _sess;

		struct snmp_pdu *pdu = NULL;
		struct snmp_pdu *response = NULL;

		// prepare and initialize snmp session;
		snmp_sess_init(&_sess);

		_sess.version = SNMP_VERSION_2c;
		_sess.peername = strdup(_current_local-> host);
		_sess.community = strdup(_current_local -> community);
		_sess.community_len = strlen(_sess.community);

		if (!(_host_ss -> _sess = snmp_sess_open(&_sess))) {

			sprintf(
				_log_str,
				"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
				"error",
				worker -> index,
				STAT_ERROR,
				_current_local -> host,
				_current_local -> objoid,
				snmp_api_errstring(snmp_errno)
			);
			log2me(DEVELOP, _log_str);

// ts2(_log_str);
//, snmp_api_errstring(snmp_errno)
// printf("[error] %s!\n", snmp_api_errstring(snmp_errno));
// exit(-1);
// snmp_perror(snmp_errno);

			// FIXME: consider this code
//		if (sessp != NULL)
//			status = snmp_sess_synch_response(sessp, pdu, &response);
//		else
//			status = STAT_DESCRIP_ERROR;

			continue;

		}

		// make our OID snmp lib friendly;
		_host_ss -> _oid_name = strdup(_current_local -> objoid);
		_host_ss -> _oid_len = sizeof(_host_ss -> _oid) / sizeof(_host_ss -> _oid[0]);

		if (!read_objid(_host_ss -> _oid_name, _host_ss -> _oid, &_host_ss -> _oid_len)) {

			// FIXME: still to be handled when happends;
			snmp_perror("read_objid");
			exit(-1);

		}

		// send the first GET
		pdu = snmp_pdu_create(SNMP_MSG_GET);
		snmp_add_null_var(pdu, _host_ss -> _oid, _host_ss -> _oid_len);

		sprintf(
			_log_str,
			"[%8s] tid:%4d, status:%3d, host+oid: %s+%s, result: %s",
			"info",
			worker -> index,
			STAT_SUCCESS,
			_current_local -> host,
			_current_local -> objoid,
			"sending request"
		);
		log2me(DEVELOP, _log_str);

		gettimeofday(&_now, NULL);
		_current_local -> _ts1_tv_sec = _now.tv_sec;
		_current_local -> _ts1_tv_usec = _now.tv_usec;

		int _status = snmp_sess_synch_response(_host_ss -> _sess, pdu, &response);
		
		
		// Collect response and process stats
		// NOTE: and order a little our responce dumps;
		// when we have a snmp result, updating a starts counters;
		// int print_result (int status, struct snmp_session *sp, struct snmp_pdu *pdu)

		gettimeofday(&_now, NULL);
		_current_local -> _ts2_tv_sec = _now.tv_sec;
		_current_local -> _ts2_tv_usec = _now.tv_usec;

		PT_MUTEX_LOCK(&stats.mutex);
		_buffered_print_result(_current_local, worker -> index, _host_ss, _status, &_sess, response);
		PT_MUTEX_UNLOCK(&stats.mutex);

		// NOTE: important this is!
		snmp_free_pdu(response);
		snmp_sess_close(_host_ss -> _sess);
		free(_host_ss);

	} // for (;;)

}

/*

 code smells <<<

//		PT_MUTEX_LOCK(&crew->mutex);
//		current = getNext();
//		if (status == STAT_SUCCESS) entry->last_value = result;
//		if (init == NEW) entry->init = LIVE;
//		 signal for our control thread;
//		 but should also check for number of threads completed;
//		PT_MUTEX_UNLOCK(&crew->mutex);
//		while (current == NULL) {
//			PT_COND_WAIT(&crew -> go, &crew -> mutex);
//			_current_local = NULL;
//		}
//		if (current != NULL) {
//			_current_local = current;
//			// printf("[ info] thread [%d] work_count index: %d\n", worker->index, crew -> _send_work_count);
//		}
//		} else if (_current_local == NULL) {
//			PT_COND_WAIT(&crew->go, &crew->mutex);
//			// PT_MUTEX_UNLOCK(&crew -> mutex);
//			continue;
//			// return 0;
//		} else if (crew->_sent_work_count <= 0) {
//			PT_COND_BROAD(&crew->_sending_done);
//			// PT_MUTEX_UNLOCK(&crew -> mutex);
//			continue;
//			// return 0;

 // simple structured printing of returned data;

int print_result(int status, struct snmp_session *sp, struct snmp_pdu *pdu)
{
	char buf[1024];
	struct variable_list *vp;
	int ix;

	struct timeval now;
	struct timezone tz;
	struct tm *tm;

	gettimeofday(&now, &tz);
	tm = localtime(&now.tv_sec);

	fprintf(stdout, "%.2d:%.2d:%.2d.%.6d, status: %d, ", tm->tm_hour, tm->tm_min, tm->tm_sec,
	        now.tv_usec, status);

	switch (status) {
		case STAT_SUCCESS:
			vp = pdu->variables;
			if (pdu->errstat == SNMP_ERR_NOERROR) {
				while (vp) {
					snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
					fprintf(stdout, ":%s: %s\n", sp->peername, buf);
					vp = vp->next_variable;
				}
			}
			else {
				for (ix = 1; vp && ix != pdu->errindex; vp = vp->next_variable, ix++)
					;
				if (vp) snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
				else strcpy(buf, "(none)");
				fprintf(stdout, ":%s: %s: %s\n",
				        sp->peername, buf, snmp_errstring(pdu->errstat));
			}
			return 1;
		case STAT_TIMEOUT:
			fprintf(stdout, ":%s: Timeout\n", sp -> peername);
			return 0;
		case STAT_ERROR:
			snmp_perror(sp->peername);
			return 0;
	}
	return 0;
}

*/