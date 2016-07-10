//
// Created by Hristo Hristov on 7/5/16.
//

/****************************************************************************
   Program:     $Id: rtgpoll.c,v 1.22 2003/09/25 15:56:04 rbeverly Exp $
   Author:      $Author: rbeverly $
   Date:        $Date: 2003/09/25 15:56:04 $
   Description: RTG SNMP get dumps to MySQL database
****************************************************************************/

#define _REENTRANT

// #include "common.h"

#include "rtg.h"

#include <syslog.h>

/* Yes.  Globals. */
stats_t stats = {PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0, 0, 0, 0, 0, 0.0};

char *target_file = NULL;
target_t *current = NULL;

int entries = 0;

MYSQL mysql;

/* dfp is a debug file pointer.  Points to stderr unless debug=level is set */
FILE *_fp_debug = NULL;

int _async_global_recv_work_count = 0;

/*
 *
 * main rtg.phoenix.poller - namely ppoller event loop;
 *
 */

int main(int argc, char *argv[]) {

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

	 *
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
				set.dboff = TRUE;
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
				set.withzeros = TRUE;
				break;
		}

	}

//	printf("%d, %d", set.verbose, OFF);
//	exit(0);

	if (set.verbose >= LOW) {
		printf("[ info] %s version %s is starting...\n", RTG_NAME, RTG_VERSION);
	}

	/* Initialize signal handler */
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

	/* Initialize the SNMP session */
	if (set.verbose >= LOW)
		printf("Initializing SNMP (v%d, port %d).\n", set.snmp_ver, set.snmp_port);

	// snmp lib;
	init_snmp(RTG_NAME_POLLER);

	/* Attempt to connect to the MySQL Database */
	#ifndef _DB_BACKEND_DISABLED
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
	#endif

	if (set.verbose >= HIGH)
		printf("\nStarting threads.\n");

	// current = NULL;

	// worker threads...
	for (i = 0; i < set.threads; i++) {
		crew.member[i].index = i;
		crew.member[i].crew = &crew;
		if (pthread_create(&(crew.member[i].thread), NULL, sync_poller, (void *) &(crew.member[i])) != 0)
			printf("pthread_create error\n");
	}

//	// signal handler thread...
//	if (pthread_create(&_reader_thread, NULL, async_reader, (void *) &crew) != 0)
//		printf("pthread_create error\n");

	// signal handler thread...
	if (pthread_create(&sig_thread, NULL, sig_handler, (void *) &(signal_set)) != 0)
		printf("pthread_create error\n");

	/* give threads time to start up */
	// ?! wtf sleep(1);

	if (set.verbose >= LOW)
		ts2("RTG Ready.\n");

	/* Loop Forever Polling Target List */
	while (1) {

		lock = TRUE;
		gettimeofday(&now, NULL);
		begin_time = (double) now.tv_usec / 1000000 + now.tv_sec;

		PT_MUTEX_LOCK(&(crew.mutex));
		init_hash_walk();
		current = NULL;
		// getNext();
		crew._send_work_count = entries;
		crew._send_worker_count = set.threads;
		// crew._recv_work_count = 0;
		// _async_global_recv_work_count = 0;
		PT_MUTEX_UNLOCK(&(crew.mutex));

		if (set.verbose >= LOW)
			ts2("[ info] Queue ready, broadcasting thread GO condition.");

		// http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_cond_broadcast.html
		PT_MUTEX_LOCK(&(crew.mutex));
		PT_COND_BROAD(&(crew.go));
		PT_MUTEX_UNLOCK(&(crew.mutex));

		// waiting fro all work units;
		PT_MUTEX_LOCK(&(crew.mutex));
		while (crew._send_worker_count > 0) {
			PT_COND_WAIT(&(crew._sending_done), &(crew.mutex));
		}
		PT_MUTEX_UNLOCK(&(crew.mutex));

		if (set.verbose >= LOW)
			ts2("[ info] All data received, broadcasting thread GO.LOG condition.");

//		PT_MUTEX_LOCK(&(crew.mutex));
//		// while (crew._recv_work_count > 0) {
//		PT_COND_WAIT(&(crew._recv_done), &(crew.mutex));
//		// }
//		PT_MUTEX_UNLOCK(&(crew.mutex));

		gettimeofday(&now, NULL);
		lock = FALSE;

		end_time = (double) now.tv_usec / 1000000 + now.tv_sec;
		stats.poll_time = end_time - begin_time;
		stats.round++;

		sleep_time = set.interval - stats.poll_time;

		if (waiting) {
			if (set.verbose >= HIGH)
				printf("Processing pending SIGHUP.\n");
			entries = hash_target_file(target_file);
			waiting = FALSE;
		}

		if (set.verbose >= LOW) {
			snprintf(errstr, sizeof(errstr), "[ info] Poll round %d complete.", stats.round);
			// syslog(LOG_INFO, errstr);
			ts2(errstr);
			print_stats(stats);
		}

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
				_db_disconnect(&mysql);
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
