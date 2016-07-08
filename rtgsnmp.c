/****************************************************************************
   Program:     $Id: rtgsnmp.c,v 1.24 2003/09/25 18:23:35 rbeverly Exp $
   Author:      $Author: rbeverly $
   Date:        $Date: 2003/09/25 18:23:35 $
   Description: RTG SNMP Routines
****************************************************************************/

// #include "common.h"

// #include "asn1.h"
// #include "snmp_api.h"
// #include "snmp_impl.h"
// #include "snmp_client.h"
// #include "mib.h"
// #include "snmp.h"

#include "rtg.h"

// global pointer to our OID hash;
extern target_t *current;

extern stats_t stats;
extern MYSQL mysql;

extern int _async_global_recv_work_count;


/*
 *
 * 'll help me to ... poll all hosts in parallel ;)
 *
 */
struct session {

	/* SNMP session data */
	struct snmp_session *_sess;

	/* How far in our poll are we */
	const char *_oid_name;
	oid _oid[MAX_OID_LEN];
	int _oid_len;

};


/*
 * simple printing of returned data
 */
int print_result (int status, struct snmp_session *sp, struct snmp_pdu *pdu)
{
	char buf[1024];
	struct variable_list *vp;
	int ix;
	struct timeval now;
	struct timezone tz;
	struct tm *tm;

	gettimeofday(&now, &tz);
	tm = localtime(&now.tv_sec);
	fprintf(stdout, "%.2d:%.2d:%.2d.%.6d ", tm->tm_hour, tm->tm_min, tm->tm_sec,
	        now.tv_usec);
	switch (status) {
		case STAT_SUCCESS:
			vp = pdu->variables;
			if (pdu->errstat == SNMP_ERR_NOERROR) {
				while (vp) {
					snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
					fprintf(stdout, "%s: %s\n", sp->peername, buf);
					vp = vp->next_variable;
				}
			}
			else {
				for (ix = 1; vp && ix != pdu->errindex; vp = vp->next_variable, ix++)
					;
				if (vp) snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
				else strcpy(buf, "(none)");
				fprintf(stdout, "%s: %s: %s\n",
				        sp->peername, buf, snmp_errstring(pdu->errstat));
			}
			return 1;
		case STAT_TIMEOUT:
			fprintf(stdout, "%s: Timeout\n", sp->peername);
			return 0;
		case STAT_ERROR:
			snmp_perror(sp->peername);
			return 0;
	}
	return 0;
}

/*
 * response handler
 */

int asynch_response(
	int operation,
	struct snmp_session *sp,
	int reqid,
	struct snmp_pdu *pdu,
	void *magic)
{

	struct session *host = (struct session *)magic;
	struct snmp_pdu *req;

	printf("[ info] async_response() call...\n");

	if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {

		if (print_result(STAT_SUCCESS, host -> _sess, pdu)) {
			// host->current_oid++;			/* send next GET (if any) */
			// one OID per host for now; (highly uneffective)
			/*if (host->current_oid->Name) {
				req = snmp_pdu_create(SNMP_MSG_GET);
				snmp_add_null_var(req, host->current_oid->Oid, host->current_oid->OidLen);
				if (snmp_send(host->sess, req))
					return 1;
				else {
					snmp_perror("snmp_send");
					snmp_free_pdu(req);
				}
			}
			*/
		}

	} else {

		print_result(STAT_TIMEOUT, host->_sess, pdu);

	}
	_async_global_recv_work_count --;
	// and probably got counter down;
//	snmp_sess_close(host -> _sess);
	printf("[ info] async_responce() host -> sess: %llu\n",  host -> _sess);
	snmp_close(host -> _sess);

	/*
	 * something went wrong (or end of variables)
	 * this host not active any more
	 */

//	active_hosts--;

	return 1;

}

void *async_poller(void *thread_args) {

	// phoenix async poller;
	worker_t *worker = (worker_t *) thread_args;
	crew_t *crew = worker -> crew;
	// target_t *entry = NULL;

	for( ; ; ) {

		PT_MUTEX_LOCK(&crew->mutex);

		while (current == NULL) {
			PT_COND_WAIT(&crew->go, &crew->mutex);
		}

		if (current != NULL) {

			printf("[ info] thread [%d] work_count index: %d\n", worker->index, crew -> _send_work_count);

			// preparing snmp session;
			// we got what we need from current entry
			// moving to next entry for other wating threads;

//			printf(
//				"[ info] thread [%d] processing -> host: %s, oid: %s\n",
//				worker -> index,
//				current -> host,
//			    current -> objoid
//			);

			// making a snmp session ...

			// struct session *_host_ss = stuepd you are, this is a ptr! sohuld be initialized 1st :- ;
			struct session *_host_ss = calloc(1, sizeof(struct session));
			// stuepd you are, this is a ptr! sohuld be initialized 1st :- ;
			// struct host *hp;

			/* startup all hosts */

			// for (hs = sessions, hp = hosts; hp->name; hs++, hp++) {

			struct snmp_pdu *req;

			struct snmp_session sess;

			snmp_sess_init(&sess);

			/* initialize session */
			sess.version = SNMP_VERSION_2c;
			sess.peername = strdup(current -> host);
			sess.community = strdup(current -> community);
			sess.community_len = strlen(sess.community);

			/* default callback */
			sess.callback = asynch_response;
			sess.callback_magic = _host_ss;

			// if (!(_host_ss -> _sess = snmp_sess_open(&sess))) {
			if (!(_host_ss -> _sess = snmp_open(&sess))) {

				//, snmp_api_errstring(snmp_errno)
				printf("[error] %s!!!\n", snmp_api_errstring(snmp_errno));
				// snmp_perror(snmp_errno);
				continue;

			}

			printf("[ info] thread [%d] &sess: %llu, _host_ss -> _sess: %llu\n", worker->index,
			       &sess, _host_ss -> _sess);

//			struct snmp_session *_ss_ptr = snmp_sess_session(_host_ss -> _sess);

			_host_ss -> _oid_name = strdup(current -> objoid);
			// also translate this in to snmp format;
			_host_ss -> _oid_len = sizeof(_host_ss -> _oid) / sizeof(_host_ss -> _oid[0]);

			if (!read_objid(_host_ss -> _oid_name, _host_ss -> _oid, &_host_ss -> _oid_len)) {
				snmp_perror("read_objid");
				exit(1);
			}

			req = snmp_pdu_create(SNMP_MSG_GET);	/* send the first GET */

			snmp_add_null_var(req, _host_ss -> _oid, _host_ss -> _oid_len);


//			//if (snmp_sess_send(_host_ss -> _sess, req)) {
			if (snmp_send(_host_ss -> _sess, req)) {

				crew -> _recv_work_count++;
				_async_global_recv_work_count++;

				// active_hosts++;
				// have something to wait for;
				;
			} else {
				snmp_perror("snmp_send");
				snmp_free_pdu(req);
			}

			// but leaking ...
			// snmp_sess_close(_host_ss -> _sess);

			current = getNext();

			PT_COND_BROAD(&crew->_go_recv);
			// snmp_close(_host_ss -> _sess);

		}

		PT_MUTEX_UNLOCK(&crew->mutex);

		// sending snmp session;

		/* Collect response and process stats */
		PT_MUTEX_LOCK(&stats.mutex);
		// when we have a snmp result, updating a starts counters;
		PT_MUTEX_UNLOCK(&stats.mutex);

		// analyzing the result;
		// and making it look like correct one (limits and correctness check);

		// if out of range for example we have the following stats update;
		// PT_MUTEX_LOCK(&stats.mutex);
		// stats.out_of_range++;
		// PT_MUTEX_UNLOCK(&stats.mutex);

		// closing snmp session;

		// decreasing work counter;

		PT_MUTEX_LOCK(&crew->mutex);
		crew->_send_work_count--;

//		if (status == STAT_SUCCESS) entry->last_value = result;
//		if (init == NEW) entry->init = LIVE;

		// signal for our control thread;
		if (crew->_send_work_count <= 0) {
			// if (set.verbose >= HIGH) printf("Queue processed. Broadcasting thread done condition.\n");
			PT_COND_BROAD(&crew->_send_done);
		}
//
//		if (set.verbose >= DEVELOP)
//			printf("Thread [%d] unlocking (update work_count)\n", worker->index);

		PT_MUTEX_UNLOCK(&crew->mutex);

	} // for (;;)

}

void *async_reader(void *thread_args) {

	// phoenix async poller;
	// worker_t *worker = (worker_t *) thread_args;
	crew_t *crew = (crew_t *) thread_args;

	// target_t *entry = NULL;

	for( ; ; ) {

		PT_MUTEX_LOCK(&crew->mutex);

		while (_async_global_recv_work_count == 0) {
			// broadcast recv_done by main thread
			printf("[ info] thread [async_reader()] boradcasting _recv_done !!!, _recv_work_count index: %d\n", _async_global_recv_work_count);
			PT_COND_BROAD(&crew->_recv_done);
			PT_COND_WAIT(&crew->_go_recv, &crew->mutex);
		}
		printf("[ info] thread [async_reader()] _recv_work_count index: %d\n", _async_global_recv_work_count);

		int fds = 0, block = 1;
		fd_set fdset;
		struct timeval timeout;

		FD_ZERO(&fdset);
		snmp_select_info(&fds, &fdset, &timeout, &block);
		// snmp_sess_select_info(&fds, &fdset, &timeout, &block);

		fds = select(fds, &fdset, NULL, NULL, block ? NULL : &timeout);
		if (fds < 0) {
			perror("select failed");
			exit(1);
		}

		/*
		 * void snmp_read(fdset)
		 *     fd_set  *fdset;
		 *
		 * Checks to see if any of the fd's set in the fdset belong to
		 * snmp.  Each socket with it's fd set has a packet read from it
		 * and snmp_parse is called on the packet received.  The resulting pdu
		 * is passed to the callback routine for that session.  If the callback
		 * routine returns successfully, the pdu and it's request are deleted.
		 */

		if (fds) {

			snmp_read(&fdset);
			// crew -> _recv_work_count--;

		} else {

			snmp_timeout();

		}

		PT_MUTEX_UNLOCK(&crew->mutex);

	}


//	struct session *hs;
//	struct host *hp;
//
//	/* loop while any active hosts */
//
//	while (active_hosts) {
//
//	}
//
//	/* cleanup */
//
//	for (hp = hosts, hs = sessions; hp->name; hs++, hp++) {
//		if (hs->sess) snmp_close(hs->sess);
//	}

}

void *poller2(void *thread_args) {

	worker_t *worker = (worker_t *) thread_args;

	crew_t *crew = worker -> crew;
	target_t *entry = NULL;

	void *_sess_ptr = NULL;

	// local snmp session struct;
	struct snmp_session session;

	struct snmp_pdu *pdu = NULL;
	struct snmp_pdu *response = NULL;

	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;

	struct variable_list *vars = NULL;
	unsigned long long result = 0;
	unsigned long long last_value = 0;
	unsigned long long insert_val = 0;
	int status = 0, bits = 0, init = 0;

	char query[_BUFF_SIZE];
	char storedoid[_BUFF_SIZE];
	char result_string[_BUFF_SIZE];

	if (set.verbose >= HIGH)
		printf("Thread [%d] starting.\n", worker->index);

	#ifndef _DB_BACKEND_DISABLED
	mysql_thread_init();
	#endif

//	if (MYSQL_VERSION_ID > 40000)
//		mysql_thread_init();
//	else
//		my_thread_init();

	while (1) {

	if (set.verbose >= DEVELOP)
		printf("Thread [%d] locking (wait on work)\n", worker->index);

	PT_MUTEX_LOCK(&crew->mutex);

	while (current == NULL) {
		PT_COND_WAIT(&crew->go, &crew->mutex);
	}

	if (set.verbose >= DEVELOP)
		printf("Thread [%d] done waiting, received go (work cnt: %d)\n", worker->index, crew -> _send_work_count);

	if (current != NULL) {

		if (set.verbose >= HIGH)
			printf("Thread [%d] processing %s %s (%d work units remain in queue)\n", worker->index, current->host,
			       current->objoid, crew->_send_work_count);

		// initialize session struct;
		snmp_sess_init(&session);

		if (set.snmp_ver == 2)
			session.version = SNMP_VERSION_2c;
		else
			session.version = SNMP_VERSION_1;

		session.peername = current->host;
		session.remote_port = set.snmp_port;
		session.community = (u_char *) current->community;
		session.community_len = strlen(session.community);

		// create it;
		_sess_ptr = snmp_sess_open(&session);

		anOID_len = MAX_OID_LEN;
		pdu = snmp_pdu_create(SNMP_MSG_GET);

		read_objid(current->objoid, anOID, &anOID_len);
		entry = current;
		last_value = current->last_value;
		init = current->init;
		insert_val = 0;
		bits = current->bits;

		strncpy(storedoid, current->objoid, sizeof(storedoid));

		current = getNext();
	}

	if (set.verbose >= DEVELOP)
		printf("Thread [%d] unlocking (done grabbing current)\n", worker->index);

	PT_MUTEX_UNLOCK(&crew->mutex);

	// http://www.net-snmp.org/wiki/index.php/TUT:Simple_Application
	snmp_add_null_var(pdu, anOID, anOID_len);

	if (_sess_ptr != NULL)
		status = snmp_sess_synch_response(_sess_ptr, pdu, &response);
	else
		status = STAT_DESCRIP_ERROR;

	/* Collect response and process stats */
	PT_MUTEX_LOCK(&stats.mutex);

	if (status == STAT_DESCRIP_ERROR) {
		stats.errors++;
		printf("*** SNMP Error: (%s) Bad descriptor.\n", session.peername);
	} else if (status == STAT_TIMEOUT) {
		stats.no_resp++;
		printf("*** SNMP No response: (%s@%s).\n", session.peername,
		       storedoid);
	} else if (status != STAT_SUCCESS) {
		stats.errors++;
		printf("*** SNMP Error: (%s@%s) Unsuccessuful (%d).\n", session.peername,
		       storedoid, status);
	} else if (status == STAT_SUCCESS && response->errstat != SNMP_ERR_NOERROR) {
		stats.errors++;
		printf("*** SNMP Error: (%s@%s) %s\n", session.peername,
		       storedoid, snmp_errstring(response->errstat));
	} else if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
		stats.polls++;
	}

	PT_MUTEX_UNLOCK(&stats.mutex);

	/* Liftoff, successful poll, process it */
	if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
		vars = response->variables;
//#ifdef OLD_UCD_SNMP
//			sprint_value(result_string, anOID, anOID_len, vars);
//#else

		snprint_value(result_string, _BUFF_SIZE, anOID, anOID_len, vars);

//#endif
		switch (vars->type) {

			/*
			 * Switch over vars->type and modify/assign result accordingly.
			 */

			case ASN_COUNTER64:
				if (set.verbose >= DEBUG)
					printf("64-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
				result = vars->val.counter64->high;
				result = result << 32;
				result = result + vars->val.counter64->low;
				break;

			case ASN_COUNTER:
				if (set.verbose >= DEBUG)
					printf("32-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
				result = (unsigned long) *(vars->val.integer);
				break;

			case ASN_INTEGER:
				if (set.verbose >= DEBUG)
					printf("Integer result: (%s@%s) %s\n", session.peername, storedoid, result_string);
				result = (unsigned long) *(vars->val.integer);
				break;

			case ASN_GAUGE:
				if (set.verbose >= DEBUG)
					printf("32-bit gauge: (%s@%s) %s\n", session.peername, storedoid, result_string);
				result = (unsigned long) *(vars->val.integer);
				break;

			case ASN_TIMETICKS:
				if (set.verbose >= DEBUG)
					printf("Timeticks result: (%s@%s) %s\n", session.peername, storedoid, result_string);
				result = (unsigned long) *(vars->val.integer);
				break;

			case ASN_OPAQUE:
				if (set.verbose >= DEBUG)
					printf("Opaque result: (%s@%s) %s\n", session.peername, storedoid, result_string);
				result = (unsigned long) *(vars->val.integer);
				break;

			default:
				if (set.verbose >= DEBUG)
					printf("Unknown result type: (%s@%s) %s\n", session.peername, storedoid, result_string);

		}

		/* Gauge Type */
		if (bits == 0) {
			if (result != last_value) {
				insert_val = result;
				if (set.verbose >= HIGH)
					printf("Thread [%d]: Gauge change from %lld to %lld\n", worker->index, last_value, insert_val);
			} else {
				if (set.withzeros)
					insert_val = result;
				if (set.verbose >= HIGH)
					printf("Thread [%d]: Gauge steady at %lld\n", worker->index, insert_val);
			}
			/* Counter Wrap Condition */
		} else if (result < last_value) {

			PT_MUTEX_LOCK(&stats.mutex);
			stats.wraps++;
			PT_MUTEX_UNLOCK(&stats.mutex);

			if (bits == 32) insert_val = (_THIRTY_TWO - last_value) + result;
			else if (bits == 64) insert_val = (_SIXTY_FOUR - last_value) + result;

			if (set.verbose >= LOW) {
				printf("*** Counter Wrap (%s@%s) [poll: %llu][last: %llu][insert: %llu]\n",
				       session.peername, storedoid, result, last_value, insert_val);
			}

			/* Not a counter wrap and this is not the first poll */

		} else if ((last_value >= 0) && (init != NEW)) {

			insert_val = result - last_value;
			/* Print out SNMP result if verbose */
			if (set.verbose == DEBUG)
				printf("Thread [%d]: (%lld-%lld) = %llu\n", worker->index, result, last_value, insert_val);
			if (set.verbose == HIGH)
				printf("Thread [%d]: %llu\n", worker->index, insert_val);
			/* last_value < 0, so this must be the first poll */

		} else {

			if (set.verbose >= HIGH) printf("Thread [%d]: First Poll, Normalizing\n", worker->index);

		}

/* Check for bogus data, either negative or unrealistic */

/* 
        sooooooo, let's fuck up the poller!!!! 
*/

//	    if (insert_val > entry->maxspeed || result < 0) {

		if (insert_val > set.out_of_range || result < 0) {

			if (set.verbose >= LOW)
				printf(
						"*** Out of Range (%s@%s) [insert_val: %llu] [oor: %lld] [set.out_of_range: %lld]\n",
						session.peername,
						storedoid,
						insert_val,
						entry->maxspeed,
						set.out_of_range);

			insert_val = 0;

			PT_MUTEX_LOCK(&stats.mutex);
			stats.out_of_range++;
			PT_MUTEX_UNLOCK(&stats.mutex);

		}

		#ifndef _DB_BACKEND_DISABLED
		if (!(set.dboff)) {

			if ((insert_val > 0) || (set.withzeros)) {

				PT_MUTEX_LOCK(&crew->mutex);

				snprintf(query, sizeof(query), "INSERT INTO %s VALUES (%d, NOW(), %llu)",
				         entry->table, entry->iid, insert_val);

				if (set.verbose >= DEBUG) printf("SQL: %s\n", query);
				status = mysql_query(&mysql, query);
				if (status) printf("*** MySQL Error: %s\n", mysql_error(&mysql));
				PT_MUTEX_UNLOCK(&crew->mutex);

				if (!status) {
					PT_MUTEX_LOCK(&stats.mutex);
					stats.db_inserts++;
					PT_MUTEX_UNLOCK(&stats.mutex);
				}
			}
			/* insert_val > 0 or withzeros */
		} /* !dboff */
		#endif

	} /* STAT_SUCCESS */

	if (_sess_ptr != NULL) {
		snmp_sess_close(_sess_ptr);
		if (response != NULL) snmp_free_pdu(response);
	}

	if (set.verbose >= DEVELOP)
		printf("Thread [%d] locking (update work_count)\n", worker->index);

	PT_MUTEX_LOCK(&crew->mutex);
	crew->_send_work_count--;
	/* Only if we received a positive result back do we update the
	   last_value object */

	if (status == STAT_SUCCESS) entry->last_value = result;
	if (init == NEW) entry->init = LIVE;
	if (crew->_send_work_count <= 0) {
		if (set.verbose >= HIGH) printf("Queue processed. Broadcasting thread done condition.\n");
		PT_COND_BROAD(&crew->_send_done);
	}

	if (set.verbose >= DEVELOP)
		printf("Thread [%d] unlocking (update work_count)\n", worker->index);

	PT_MUTEX_UNLOCK(&crew->mutex);

	}
	/* while(1) */

}

void *poller(void *thread_args) {

	worker_t *worker = (worker_t *) thread_args;

	crew_t *crew = worker->crew;
	target_t *entry = NULL;
	void *sessp = NULL;
	struct snmp_session session;
	struct snmp_pdu *pdu = NULL;
	struct snmp_pdu *response = NULL;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	struct variable_list *vars = NULL;
	unsigned long long result = 0;
	unsigned long long last_value = 0;
	unsigned long long insert_val = 0;
	int status = 0, bits = 0, init = 0;

	char query[_BUFF_SIZE];
	char storedoid[_BUFF_SIZE];
	char result_string[_BUFF_SIZE];

	if (set.verbose >= HIGH)
		printf("Thread [%d] starting.\n", worker->index);
	if (MYSQL_VERSION_ID > 40000)
		mysql_thread_init();
	else
		my_thread_init();

	while (1) {

		if (set.verbose >= DEVELOP)
			printf("Thread [%d] locking (wait on work)\n", worker->index);

		PT_MUTEX_LOCK(&crew->mutex);

		while (current == NULL) {
			PT_COND_WAIT(&crew->go, &crew->mutex);
		}
		if (set.verbose >= DEVELOP)
			printf("Thread [%d] done waiting, received go (work cnt: %d)\n", worker->index, crew->_send_work_count);

		if (current != NULL) {
			if (set.verbose >= HIGH)
				printf("Thread [%d] processing %s %s (%d work units remain in queue)\n", worker->index, current->host,
				       current->objoid, crew->_send_work_count);
			snmp_sess_init(&session);
			if (set.snmp_ver == 2)
				session.version = SNMP_VERSION_2c;
			else
				session.version = SNMP_VERSION_1;
			session.peername = current->host;
			session.remote_port = set.snmp_port;
			session.community = current->community;
			session.community_len = strlen(session.community);

			sessp = snmp_sess_open(&session);
			anOID_len = MAX_OID_LEN;
			pdu = snmp_pdu_create(SNMP_MSG_GET);
			read_objid(current->objoid, anOID, &anOID_len);
			entry = current;
			last_value = current->last_value;
			init = current->init;
			insert_val = 0;
			bits = current->bits;
			strncpy(storedoid, current->objoid, sizeof(storedoid));
			current = getNext();
		}
		if (set.verbose >= DEVELOP)
			printf("Thread [%d] unlocking (done grabbing current)\n", worker->index);
		PT_MUTEX_UNLOCK(&crew->mutex);

		snmp_add_null_var(pdu, anOID, anOID_len);
		if (sessp != NULL)
			status = snmp_sess_synch_response(sessp, pdu, &response);
		else
			status = STAT_DESCRIP_ERROR;

		/* Collect response and process stats */
		PT_MUTEX_LOCK(&stats.mutex);
		if (status == STAT_DESCRIP_ERROR) {
			stats.errors++;
			printf("*** SNMP Error: (%s) Bad descriptor.\n", session.peername);
		} else if (status == STAT_TIMEOUT) {
			stats.no_resp++;
			printf("*** SNMP No response: (%s@%s).\n", session.peername,
			       storedoid);
		} else if (status != STAT_SUCCESS) {
			stats.errors++;
			printf("*** SNMP Error: (%s@%s) Unsuccessuful (%d).\n", session.peername,
			       storedoid, status);
		} else if (status == STAT_SUCCESS && response->errstat != SNMP_ERR_NOERROR) {
			stats.errors++;
			printf("*** SNMP Error: (%s@%s) %s\n", session.peername,
			       storedoid, snmp_errstring(response->errstat));
		} else if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
			stats.polls++;
		}
		PT_MUTEX_UNLOCK(&stats.mutex);

		/* Liftoff, successful poll, process it */
		if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
			vars = response->variables;
#ifdef OLD_UCD_SNMP
			sprint_value(result_string, anOID, anOID_len, vars);
#else
			snprint_value(result_string, _BUFF_SIZE, anOID, anOID_len, vars);
#endif
			switch (vars->type) {
				/*
				 * Switch over vars->type and modify/assign result accordingly.
				 */
				case ASN_COUNTER64:
					if (set.verbose >= DEBUG)
						printf("64-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
					result = vars->val.counter64->high;
					result = result << 32;
					result = result + vars->val.counter64->low;
					break;
				case ASN_COUNTER:
					if (set.verbose >= DEBUG)
						printf("32-bit result: (%s@%s) %s\n", session.peername, storedoid, result_string);
					result = (unsigned long) *(vars->val.integer);
					break;
				case ASN_INTEGER:
					if (set.verbose >= DEBUG)
						printf("Integer result: (%s@%s) %s\n", session.peername, storedoid, result_string);
					result = (unsigned long) *(vars->val.integer);
					break;
				case ASN_GAUGE:
					if (set.verbose >= DEBUG)
						printf("32-bit gauge: (%s@%s) %s\n", session.peername, storedoid, result_string);
					result = (unsigned long) *(vars->val.integer);
					break;
				case ASN_TIMETICKS:
					if (set.verbose >= DEBUG)
						printf("Timeticks result: (%s@%s) %s\n", session.peername, storedoid, result_string);
					result = (unsigned long) *(vars->val.integer);
					break;
				case ASN_OPAQUE:
					if (set.verbose >= DEBUG)
						printf("Opaque result: (%s@%s) %s\n", session.peername, storedoid, result_string);
					result = (unsigned long) *(vars->val.integer);
					break;
				default:
					if (set.verbose >= DEBUG)
						printf("Unknown result type: (%s@%s) %s\n", session.peername, storedoid, result_string);
			}

			/* Gauge Type */
			if (bits == 0) {
				if (result != last_value) {
					insert_val = result;
					if (set.verbose >= HIGH)
						printf("Thread [%d]: Gauge change from %lld to %lld\n", worker->index, last_value, insert_val);
				} else {
					if (set.withzeros)
						insert_val = result;
					if (set.verbose >= HIGH)
						printf("Thread [%d]: Gauge steady at %lld\n", worker->index, insert_val);
				}
				/* Counter Wrap Condition */
			} else if (result < last_value) {
				PT_MUTEX_LOCK(&stats.mutex);
				stats.wraps++;
				PT_MUTEX_UNLOCK(&stats.mutex);
				if (bits == 32) insert_val = (_THIRTY_TWO - last_value) + result;
				else if (bits == 64) insert_val = (_SIXTY_FOUR - last_value) + result;
				if (set.verbose >= LOW) {
					printf("*** Counter Wrap (%s@%s) [poll: %llu][last: %llu][insert: %llu]\n",
					       session.peername, storedoid, result, last_value, insert_val);
				}
				/* Not a counter wrap and this is not the first poll */
			} else if ((last_value >= 0) && (init != NEW)) {
				insert_val = result - last_value;
				/* Print out SNMP result if verbose */
				if (set.verbose == DEBUG)
					printf("Thread [%d]: (%lld-%lld) = %llu\n", worker->index, result, last_value, insert_val);
				if (set.verbose == HIGH)
					printf("Thread [%d]: %llu\n", worker->index, insert_val);
				/* last_value < 0, so this must be the first poll */
			} else {

				if (set.verbose >= HIGH) printf("Thread [%d]: First Poll, Normalizing\n", worker->index);

			}

/* Check for bogus data, either negative or unrealistic */

/*
        sooooooo, let's fuck up the poller!!!!
*/

//	    if (insert_val > entry->maxspeed || result < 0) {

			if (insert_val > set.out_of_range || result < 0) {

				if (set.verbose >= LOW)
					printf(
							"*** Out of Range (%s@%s) [insert_val: %llu] [oor: %lld] [set.out_of_range: %lld]\n",
							session.peername,
							storedoid,
							insert_val,
							entry->maxspeed,
							set.out_of_range);

				insert_val = 0;

				PT_MUTEX_LOCK(&stats.mutex);
				stats.out_of_range++;
				PT_MUTEX_UNLOCK(&stats.mutex);

			}

			if (!(set.dboff)) {
				if ((insert_val > 0) || (set.withzeros)) {
					PT_MUTEX_LOCK(&crew->mutex);
					snprintf(query, sizeof(query), "INSERT INTO %s VALUES (%d, NOW(), %llu)",
					         entry->table, entry->iid, insert_val);
					if (set.verbose >= DEBUG) printf("SQL: %s\n", query);
					status = mysql_query(&mysql, query);
					if (status) printf("*** MySQL Error: %s\n", mysql_error(&mysql));
					PT_MUTEX_UNLOCK(&crew->mutex);

					if (!status) {
						PT_MUTEX_LOCK(&stats.mutex);
						stats.db_inserts++;
						PT_MUTEX_UNLOCK(&stats.mutex);
					}
				} /* insert_val > 0 or withzeros */
			} /* !dboff */

		} /* STAT_SUCCESS */

		if (sessp != NULL) {
			snmp_sess_close(sessp);
			if (response != NULL) snmp_free_pdu(response);
		}

		if (set.verbose >= DEVELOP)
			printf("Thread [%d] locking (update work_count)\n", worker->index);
		PT_MUTEX_LOCK(&crew->mutex);
		crew->_send_work_count--;
		/* Only if we received a positive result back do we update the
		   last_value object */
		if (status == STAT_SUCCESS) entry->last_value = result;
		if (init == NEW) entry->init = LIVE;
		if (crew->_send_work_count <= 0) {
			if (set.verbose >= HIGH) printf("Queue processed. Broadcasting thread done condition.\n");
			PT_COND_BROAD(&crew->_send_done);
		}
		if (set.verbose >= DEVELOP)
			printf("Thread [%d] unlocking (update work_count)\n", worker->index);

		PT_MUTEX_UNLOCK(&crew->mutex);

	}                /* while(1) */



}
