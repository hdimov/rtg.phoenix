//
// Created by Hristo Hristov on 7/5/16.
//

/****************************************************************************
   Program:     $Id: rtgpoll.c,v 1.22 2003/09/25 15:56:04 rbeverly Exp $
   Author:      $Author: rbeverly $
   Date:        $Date: 2003/09/25 15:56:04 $
   Description: RTG SNMP get dumps to MySQL database
****************************************************************************/

#include "ppoller.h"

/*
 *
 * global data structures
 *
 * */

config_t set;
hash_t hash;

int lock;
int waiting;

stats_t stats = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0, 0, 0, 0, 0, 0.0, NULL, NULL};

char *target_file = NULL;
target_t *current = NULL;

int entries = 0;

/* dfp is a debug file pointer.  Points to stderr unless debug=level is set */
FILE *_fp_debug = NULL;

int _async_global_recv_work_count = 0;



int main(int argc, char *argv[]) {
	
//	printf("data structure sizes...\n");
//
//	printf("target_t: %d\n", sizeof(target_t) );
//	printf("queue_node_t: %d\n", sizeof(queue_node_t) );
//	printf("queue_t: %d\n", sizeof(queue_t) );
//
//	exit(0);
	
	// queue_t testin;
	
//	queue_t* _insert_q = _q_alloc();
//
//	printf("q exists? %d\n", _q_exists(_insert_q));
//	printf("q is empty? %d\n", _q_is_empty(_insert_q));
	
//	char* msgs[] = {"", "b", "c"};
//	_q_push(_insert_q, msgs[0], strlen(msgs[0]));
//	_q_push(_insert_q, msgs[1], strlen(msgs[1]));
//	_q_push(_insert_q, msgs[2], strlen(msgs[2]));
	
//	int *pi1 = (int*) malloc(sizeof(int));
//	int *pi2 = (int*) malloc(sizeof(int));
//	int *pi3 = (int*) malloc(sizeof(int));
//
//	*pi1 = 3;
//	*pi2 = 2;
//	*pi3 = 1;
//
//	_q_push(_insert_q, pi1, sizeof(int));
//	_q_push(_insert_q, pi2, sizeof(int));
//	_q_push(_insert_q, pi3, sizeof(int));
//
//	printf("count: %d\n", _insert_q -> _elem_count );
//	printf("size: %d\n", _insert_q -> _size );
//
//	while (!_q_is_empty(_insert_q)) {
//
//		int* _data = (int*) _q_pop(_insert_q);
//
//		if (_data != NULL) {
//			printf("elem: %d\n", *( (int *) _data) );
//		}
//
//	}
//
//	printf("target_t size: %d\n", sizeof(target_t) );
//	exit(0);
	
	/*
	 * syslog() aware we will be;
	 *

	 
	// http://ludovicrousseau.blogspot.bg/2015/03/change-syslog-logging-level-on-yosemite.html
	// http://man7.org/linux/man-pages/man3/syslog.3.html

	 *
	 * @macos
	 *

root@lisa ~ # ps aux | grep ppoller
root             3874   0.0  0.0  2434840    700 s000  S+    5:34PM   0:00.01 grep ppoller
lisp             3871   0.0  0.1  2508972   5060 s001  S+    5:34PM   0:00.12 ./bin/Debug/ppoller -vvvv -c rtg.phoenix.conf -t targets.conf
root@lisa ~ # syslog -c 3871
Process 3871 syslog settings: 0x00000000 OFF / 0x00 Off
root@lisa ~ # syslog -c 3871 -i
root@lisa ~ # syslog -w -k Sender rtg.phoenix.poller

//	int mask = LOG_MASK (LOG_INFO | LOG_ERR);
//	int result = setlogmask(mask);

	 */

	char *_ident = RTG_NAME_POLLER;
	int _logopt = LOG_PID; //| LOG_CONS
	int _facility = LOG_USER; //| LOG_DAEMON;

	openlog(_ident, _logopt, _facility);

	crew_t crew;
	pthread_t sig_thread;
	pthread_t _reader_thread;

	sigset_t signal_set;

	struct timeval now;

	double begin_time, end_time, sleep_time;

	char *conf_file = NULL;

	char errstr[_BUFF_SIZE];

	int ch, i;

	_fp_debug = stderr;

	// check argument count...
	if (argc < 3) usage(argv[0]);

	// set default environment...
	config_defaults(&set);

	/* Parse the command-line. */
	while ((ch = getopt(argc, argv, "c:dhmt:vz")) != EOF) {

		switch ((char) ch) {
			case 'c':
				conf_file = optarg;
				break;
			case 'd':
				set.db_off = TRUE;
				break;
			case 'h':
				usage(argv[0]);
				break;
			case 'm':
				set.multiple++;
				break;
			case 't':
				target_file = optarg;
				break;
			case 'v':
				set.verbose++;
				break;
			case 'z':
				set.with_zeros = TRUE;
				break;
		}

	}
	
	snprintf(errstr, sizeof(errstr), "%s version %s is starting! Setup and initialization steps follow.", RTG_NAME, RTG_VERSION);
	log_step_message(LOW, errstr);

	/*
	 * Initialize signal handler
	 */
	
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGHUP);
	sigaddset(&signal_set, SIGUSR1);
	sigaddset(&signal_set, SIGUSR2);
	sigaddset(&signal_set, SIGTERM);
	sigaddset(&signal_set, SIGINT);
	sigaddset(&signal_set, SIGQUIT);

	if (!set.multiple)
		checkPID(PID_FILE);

	if (pthread_sigmask(SIG_BLOCK, &signal_set, NULL) != 0)
		printf("[error] pthread_sigmask error\n");

	/*
	 *
	 * Read configuration file. Only command line -c argument will be supported by now;
	 *
	 * */

	if (conf_file) {
		if ((read_rtg_config(conf_file, &set)) < 0) {
			printf("[error] Can't read config file: %s\n", conf_file);
			exit(-1);
		}
	}

	/* else {
		conf_file = malloc(_BUFF_SIZE);
		if (!conf_file) {
			printf("Fatal malloc error!\n");
			exit(-1);
		}

		for (i = 0; i < CONFIG_PATHS; i++) {
			snprintf(conf_file, _BUFF_SIZE, "%s%s", config_paths[i], DEFAULT_CONF_FILE);
			if (read_rtg_config(conf_file, &set) >= 0) {
				break;
			}
			if (i == CONFIG_PATHS - 1) {
				snprintf(conf_file, _BUFF_SIZE, "%s%s", config_paths[0], DEFAULT_CONF_FILE);
				if ((write_rtg_config(conf_file, &set)) < 0) {
					fprintf(stderr, "Couldn't write config file.\n");
					exit(-1);
				}
			}
		}
	} */

	/* hash list of targets to be polled */
	entries = hash_target_file(target_file);
	if (entries <= 0) {
		fprintf(stderr, "Error updating target list.");
		exit(-1);
	}

	if (set.verbose >= LOW)
		printf("Initializing threads (%d).\n", set.threads);

	// signal handling and stuff shtoud be moved here;

	pthread_mutex_init(&(crew.mutex), NULL);
	pthread_cond_init(&(crew._sending_done), NULL);
	pthread_cond_init(&(crew.go), NULL);

	crew._send_work_count = 0;
	
	pthread_mutex_init(&(stats.mutex), NULL);
	pthread_cond_init(&(stats.go), NULL);
	
	/* Initialize the SNMP session */
	if (set.verbose >= LOW)
		printf("Initializing SNMP (v%d, port %d).\n", set.snmp_ver, set.snmp_port);

	// snmp lib;
	init_snmp(RTG_NAME_POLLER);

	if (set.verbose >= HIGH)
		printf("\nStarting threads.\n");
	
	// signal handler thread...
	if (pthread_create(&sig_thread, NULL, sig_handler, (void *) &(signal_set)) != 0)
		printf("pthread_create error\n");
	
	stats.logger_db_alive = FALSE;
	if (pthread_create( &(stats.logger_thread), NULL, prlogger, (void *) &(stats) ) != 0)
		printf("pthread_create error\n");
	// FIXME: a kind of barrier;
	while (stats.logger_db_alive == FALSE);
	
	// worker threads...
	for (i = 0; i < set.threads; i++) {
		crew.member[i].index = i;
		crew.member[i].crew = &crew;
		if (pthread_create(&(crew.member[i].thread), NULL, sync_poller_v2, (void *) &(crew.member[i])) != 0)
			printf("pthread_create error\n");
	}
	
	/* give threads time to start up */
	// ?! wtf sleep(1);

	log_step_message(LOW, "Setup and initialization ready.");
	
	lock = FALSE;
	
	for ( ; ; ) {
		
		gettimeofday(&now, NULL);
		begin_time = (double) now.tv_usec / 1000000 + now.tv_sec;

		PT_MUTEX_LOCK(&(crew.mutex));
		lock = TRUE;
		
		init_hash_walk();
		current = NULL;
		// getNext();
		crew._send_work_count = entries;
		crew._send_worker_count = set.threads;
		// crew._recv_work_count = 0;
		// _async_global_recv_work_count = 0;
		PT_MUTEX_UNLOCK(&(crew.mutex));
		
		lock = FALSE;
		// NOTE: may init our result queus inside stats;
		if (stats._q_result == NULL) {
			stats._q_result = _q_alloc();
		} else if (!_q_is_empty(stats._q_result)){
			log_step_message(LOW, "Result queue is not empty!!!");
		}
		log_step_message(LOW, "Data is ready, broadcasting thread GO condition.");

		// http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_cond_broadcast.html
		PT_MUTEX_LOCK(&(crew.mutex));
		lock = TRUE;
		PT_COND_BROAD(&(crew.go));
		PT_MUTEX_UNLOCK(&(crew.mutex));
		lock = FALSE;
		
		// waiting fro all work units;
		PT_MUTEX_LOCK(&(crew.mutex));
		while (crew._send_worker_count > 0) {
			// NOTE: about to be FALSE when next call happends;
			lock = FALSE;
			PT_COND_WAIT(&(crew._sending_done), &(crew.mutex));
		}
		PT_MUTEX_UNLOCK(&(crew.mutex));
		lock = FALSE;
		
		log_step_message(LOW, "All data received, checking for SIGHUP.");

//		PT_MUTEX_LOCK(&(crew.mutex));
//		// while (crew._recv_work_count > 0) {
//		PT_COND_WAIT(&(crew._recv_done), &(crew.mutex));
//		// }
//		PT_MUTEX_UNLOCK(&(crew.mutex));

		gettimeofday(&now, NULL);
		end_time = (double) now.tv_usec / 1000000 + now.tv_sec;

		stats.poll_time = end_time - begin_time;
		stats.round++;

		sleep_time = set.interval - stats.poll_time;

		if (waiting) {
			
//			if (set.verbose >= HIGH)
//				printf("Processing pending SIGHUP.\n");
			
			log_step_message(LOW, "Processing pending SIGHUP.");
			entries = hash_target_file(target_file);
			waiting = FALSE;
			
		}
		
		log_step_message(LOW, "All data received, broadcasting GO.LOG condition.");
		
		PT_MUTEX_LOCK(&(stats.mutex));

		snprintf(errstr, sizeof(errstr), "_q_result elements count: %d", stats._q_result ->_elem_count);
		log_step_message(LOW, errstr);
		// may start plogger thread;
		
		snprintf(errstr, sizeof(errstr), "Poll round %d complete.", stats.round);
		log_step_message(LOW, errstr);
		log_poll_stats(LOW, stats);
		
		PT_COND_BROAD(&(stats.go));
		PT_MUTEX_UNLOCK(&(stats.mutex));
		
		// FIXME: better logging here!
		if (sleep_time <= 0)
			stats.slow++;
		else
			sleepy(sleep_time);
		
		
	}

	/* while */

	/* Disconnect from the MySQL Database, exit. */

	#ifndef _DB_BACKEND_DISABLED
	if (!(set.dboff))
		_db_disconnect(&mysql);
	#endif

	exit(0);

}


/*
 *
 * Signal Handler.
 * USR1 increases verbosity,
 * USR2 decreases verbosity.
 * HUP re-reads target list
 *
 * */

void *sig_handler(void *arg) {

	sigset_t *signal_set = (sigset_t *) arg;
	int sig_number;

	while (1) {

		sigwait(signal_set, &sig_number);
		switch (sig_number) {
			case SIGHUP:
				if (lock) {
					waiting = TRUE;
				}
				else {
					entries = hash_target_file(target_file);
					waiting = FALSE;
				}
				break;
			case SIGUSR1:
				set.verbose++;
				break;
			case SIGUSR2:
				set.verbose--;
				break;
			case SIGTERM:
			case SIGINT:
			case SIGQUIT:
				if (set.verbose >= LOW)
					printf("Quiting: received signal %d.\n", sig_number);
				_db_disconnect(&_mysql);
				unlink(PID_FILE);
				exit(1);
				break;
		}
	}
}


void usage(char *prog) {

	printf("rtgpoll - RTG v%s\n", RTG_VERSION);
	printf("Usage: %s [-dmz] [-vvv] [-c <file>] -t <file>\n", prog);
	printf("\nOptions:\n");
	printf("  -c <file>   Specify configuration file\n");
	printf("  -d          Disable database inserts\n");
	printf("  -t <file>   Specify target file\n");
	printf("  -v          Increase verbosity\n");
	printf("  -m          Allow multiple instances\n");
	printf("  -z          Database zero delta inserts\n");
	printf("  -h          Help\n");

	exit(-1);
}
