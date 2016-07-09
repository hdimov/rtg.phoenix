
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

// extern MYSQL mysql;

/*
 * 'll help me to ... poll all hosts in parallel ;)
 */

struct session {

	/* SNMP session data */
	struct snmp_session *_sess;

	/* how far in our poll are we? */
	const char *_oid_name;

	oid _oid[MAX_OID_LEN];
	int _oid_len;

};

/*
 * simple structured printing of returned data;
 */

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

int print_result_sess(int status, struct snmp_pdu *response) {
}
/*
int print_result_buff(int status, struct snmp_session *sp, struct snmp_pdu *pdu) {

	char buf[_BUFF_SIZE];
	char _log_str[_BUFF_SIZE];

	struct variable_list *vp;
	int ix;


	fprintf(stdout, "%.2d:%.2d:%.2d.%.6d, status: %d, ", tm->tm_hour, tm->tm_min, tm->tm_sec,
	        now.tv_usec, status);

	switch (status) {
		case STAT_SUCCESS:
			vp = pdu -> variables;
			if (pdu -> errstat == SNMP_ERR_NOERROR) {
				while (vp) {
					snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
					sprintf(_log_str, "[ info] host: %s, %s\n", sp->peername, buf);
					ts2(_log_str);
					vp = vp->next_variable;
				}
			} else {
				for (ix = 1; vp && ix != pdu->errindex; vp = vp->next_variable, ix++)
					;
				if (vp) snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
				else strcpy(buf, "(none)");
//				fprintf(stdout, ":%s: %s: %s\n",
//				        sp->peername, buf, snmp_errstring(pdu->errstat));
			}
			return 1;
		case STAT_TIMEOUT:
			sprintf(_log_str, "[ info] host: %s, status: %s, Timeout\n", sp->peername, STAT_TIMEOUT);
			ts2(_log_str);
			// fprintf(stdout, ":%s: Timeout\n", sp -> peername);
			return 0;
		case STAT_ERROR:
			snmp_perror(sp->peername);
			return 0;
	}
	return 0;


}
*/
void *sync_poller(void *thread_args) {

	// phoenix async poller;
	worker_t *worker = (worker_t *) thread_args;
	crew_t *crew = worker -> crew;
	// target_t *entry = NULL;

	target_t *_current_local = NULL;

	char _log_str[_BUFF_SIZE];

	PT_MUTEX_LOCK(&crew -> mutex);
	PT_COND_WAIT(&crew->go, &crew->mutex);
	PT_MUTEX_UNLOCK(&crew -> mutex);

	for( ; ; ) {

		PT_MUTEX_LOCK(&crew -> mutex);

		current = _current_local = getNext();
		crew -> _send_work_count--;

		if (_current_local == NULL && crew->_send_work_count <= 0) {

			crew -> _send_worker_count--;

			PT_COND_BROAD(&crew->_sending_done);
			PT_COND_WAIT(&crew->go, &crew->mutex);

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

			PT_MUTEX_UNLOCK(&crew -> mutex);
			continue;
		}

		PT_MUTEX_UNLOCK(&crew -> mutex);

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
//
//		if (current != NULL) {
//			_current_local = current;
//			// printf("[ info] thread [%d] work_count index: %d\n", worker->index, crew -> _send_work_count);
//		}
		// make a copy of current and then unlock;

		// preparing snmp session;
		// we got what we need from current entry
		// moving to next entry for other wating threads;

		sprintf(_log_str,
			"[ info] thread id: %d, processing host: %s, oid: %s",
			worker -> index,
			_current_local -> host,
			_current_local -> objoid
		);
		ts2(_log_str);

		// making a snmp session ...

		// struct session *_host_ss = stuepd you are, this is a ptr! sohuld be initialized 1st :- ;
		struct session *_host_ss = calloc(1, sizeof(struct session));
		// stuepd you are, this is a ptr! sohuld be initialized 1st :- ;
		// struct host *hp;

		/* startup all hosts */
		// for (hs = sessions, hp = hosts; hp->name; hs++, hp++) {

		struct snmp_session _sess;

		struct snmp_pdu *pdu = NULL;
		struct snmp_pdu *response = NULL;

		snmp_sess_init(&_sess);

		/* initialize session */
		_sess.version = SNMP_VERSION_2c;
		_sess.peername = strdup(_current_local-> host);
		_sess.community = strdup(_current_local -> community);
		_sess.community_len = strlen(_sess.community);

//		/* default callback */
//		_sess.callback = asynch_response;
//		_sess.callback_magic = _host_ss;
//
		if (!(_host_ss -> _sess = snmp_sess_open(&_sess))) {

			//, snmp_api_errstring(snmp_errno)
			printf("[error] %s!\n", snmp_api_errstring(snmp_errno));
			// exit(-1);
			// snmp_perror(snmp_errno);
			continue;

		}

//		printf("[ info] thread [%d] &sess: %llu, _host_ss -> _sess: %llu\n", worker->index,
//		       &sess, _host_ss -> _sess);

//		struct snmp_session *_ss_ptr = snmp_sess_session(_host_ss -> _sess);

		_host_ss -> _oid_name = strdup(_current_local -> objoid);
		// also translate this in to snmp format;
		_host_ss -> _oid_len = sizeof(_host_ss -> _oid) / sizeof(_host_ss -> _oid[0]);

		if (!read_objid(_host_ss -> _oid_name, _host_ss -> _oid, &_host_ss -> _oid_len)) {
			snmp_perror("read_objid");
			exit(1);
		}

		pdu = snmp_pdu_create(SNMP_MSG_GET);	/* send the first GET */
		snmp_add_null_var(pdu, _host_ss -> _oid, _host_ss -> _oid_len);

		int _status = snmp_sess_synch_response(_host_ss -> _sess, pdu, &response);

		/* Collect response and process stats */
		// NOTE: and order a little our responce dumps;
		PT_MUTEX_LOCK(&stats.mutex);
		// int print_result (int status, struct snmp_session *sp, struct snmp_pdu *pdu)
		print_result(_status, &_sess, response);
		// when we have a snmp result, updating a starts counters;
		PT_MUTEX_UNLOCK(&stats.mutex);

		// analyzing the result;
		// and making it look like correct one (limits and correctness check);
		// if out of range for example we have the following stats update;
		// PT_MUTEX_LOCK(&stats.mutex);
		// stats.out_of_range++;
		// PT_MUTEX_UNLOCK(&stats.mutex);

		snmp_sess_close(_host_ss -> _sess);

		// decreasing work counter;

	} // for (;;)

}
