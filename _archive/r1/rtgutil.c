/****************************************************************************
   Program:     $Id: rtgutil.c,v 1.19 2003/09/25 15:56:04 rbeverly Exp $
   Author:      $Author: rbeverly $
   Date:        $Date: 2003/09/25 15:56:04 $
   Description: RTG Routines
****************************************************************************/

#include "rtg.h"

extern FILE *_fp_debug;

/* read configuration file to establish local environment */
int read_rtg_config(char *file, config_t *set) {

	FILE *fp;

	char buff[_BUFF_SIZE];
	char p1[_BUFF_SIZE];
	char p2[_BUFF_SIZE];

	if ((fp = fopen(file, "r")) == NULL) {
		return (-1);
	} else {
		if (set->verbose >= LOW)
			fprintf(_fp_debug, "Using RTG config file [%s].", file);
		while (!feof(fp)) {
			fgets(buff, _BUFF_SIZE, fp);
			if (!feof(fp) && *buff != '#' && *buff != ' ' && *buff != '\n') {
				sscanf(buff, "%20s %20s", p1, p2);
				if (!strcasecmp(p1, "Interval")) set->interval = atoi(p2);
				else if (!strcasecmp(p1, "HighSkewSlop")) set->highskewslop = atof(p2);
				else if (!strcasecmp(p1, "LowSkewSlop")) set->lowskewslop = atof(p2);
				else if (!strcasecmp(p1, "SNMP_Ver")) set->snmp_ver = atoi(p2);
				else if (!strcasecmp(p1, "SNMP_Port")) set->snmp_port = atoi(p2);
				else if (!strcasecmp(p1, "Threads")) set->threads = atoi(p2);
				else if (!strcasecmp(p1, "DB_Host")) strncpy(set->dbhost, p2, sizeof(set->dbhost));
				else if (!strcasecmp(p1, "DB_Database")) strncpy(set->dbdb, p2, sizeof(set->dbdb));
				else if (!strcasecmp(p1, "DB_User")) strncpy(set->dbuser, p2, sizeof(set->dbuser));
				else if (!strcasecmp(p1, "DB_Pass")) strncpy(set->dbpass, p2, sizeof(set->dbpass));

/* Long longs not ANSI C.  If OS doesn't support atoll() use default. */
				else if (!strcasecmp(p1, "OutOfRange"))
#ifdef HAVE_STRTOLL
					set->out_of_range = strtoll(p2, NULL, 0);
#else
					set->out_of_range = DEFAULT_OUT_OF_RANGE;
#endif

				else {
					fprintf(_fp_debug, "*** Unrecongized directive: %s=%s in %s\n",
					        p1, p2, file);
					exit(-1);
				}
			}
		}

		if (set->snmp_ver != 1 && set->snmp_ver != 2) {
			fprintf(_fp_debug, "*** Unsupported SNMP version: %d.\n", set->snmp_ver);
			exit(-1);
		}
		if (set->threads < 1 || set->threads > _MAX_THREADS) {
			fprintf(_fp_debug, "*** Invalid Number of Threads: %d (max=%d).\n",
			        set->threads, _MAX_THREADS);
			exit(-1);
		}
		return (0);
	}
}


int write_rtg_config(char *file, config_t *set) {
	FILE *fp;

	if (set->verbose >= LOW)
		fprintf(_fp_debug, "Writing default config file [%s].", file);
	if ((fp = fopen(file, "w")) == NULL) {
		fprintf(_fp_debug, "\nCould not open '%s' for writing\n", file);
		return (-1);
	} else {
		fprintf(fp, "#\n# RTG v%s Master Config\n#\n", RTG_VERSION);
		fprintf(fp, "Interval\t%d\n", set->interval);
		fprintf(fp, "HighSkewSlop\t%f\n", set->highskewslop);
		fprintf(fp, "LowSkewSlop\t%f\n", set->lowskewslop);
		fprintf(fp, "OutOfRange\t%lld\n", set->out_of_range);
		fprintf(fp, "SNMP_Ver\t%d\n", set->snmp_ver);
		fprintf(fp, "SNMP_Port\t%d\n", set->snmp_port);
		fprintf(fp, "DB_Host\t%s\n", set->dbhost);
		fprintf(fp, "DB_Database\t%s\n", set->dbdb);
		fprintf(fp, "DB_User\t%s\n", set->dbuser);
		fprintf(fp, "DB_Pass\t%s\n", set->dbpass);
		fprintf(fp, "Threads\t%d\n", set->threads);
		fclose(fp);
		return (0);
	}
}


/* Populate Master Configuration Defaults */
void config_defaults(config_t *set) {

	set->interval = DEFAULT_INTERVAL;
	set->highskewslop = DEFAULT_HIGH_SKEW_SLOP;
	set->lowskewslop = DEFAULT_LOW_SKEW_SLOP;
	set->out_of_range = DEFAULT_OUT_OF_RANGE;

	set->snmp_ver = DEFAULT_SNMP_VER;
	set->snmp_port = DEFAULT_SNMP_PORT;

	set->threads = DEFAULT_THREADS;
	strncpy(set->dbhost, DEFAULT_DB_HOST, sizeof(set->dbhost));
	strncpy(set->dbdb, DEFAULT_DB_DB, sizeof(set->dbhost));
	strncpy(set->dbuser, DEFAULT_DB_USER, sizeof(set->dbhost));
	strncpy(set->dbpass, DEFAULT_DB_PASS, sizeof(set->dbhost));
	set->dboff = FALSE;
	set->withzeros = FALSE;
	set->verbose = OFF;
/*
	strncpy(config_paths[0], CONFIG_PATH_1, sizeof(config_paths[0]));
	snprintf(config_paths[1], sizeof(config_paths[1]), "%s/etc/", RTG_HOME);
	strncpy(config_paths[2], CONFIG_PATH_2, sizeof(config_paths[1]));
*/

	return;

}


/* Print RTG stats */
void print_stats(stats_t stats) {
	printf("\n[Polls = %lld] [DBInserts = %lld] [Wraps = %d] [OutOfRange = %d]\n",
	       stats.polls, stats.db_inserts, stats.wraps, stats.out_of_range);
	printf("[No Resp = %d] [SNMP Errs = %d] [Slow = %d] [PollTime = %2.3f%c]\n",
	       stats.no_resp, stats.errors, stats.slow, stats.poll_time, 's');
	return;
}


/* A fancy sleep routine */
void sleepy(float sleep_time) {
	int chunks = 10;
	int i;

	if (sleep_time > chunks) {
		if (set.verbose >= LOW)
			printf("Next Poll: ");
		for (i = chunks; i > 0; i--) {
			if (set.verbose >= LOW) {
				printf("%d...", i);
				fflush(NULL);
			}
			usleep((unsigned int) (sleep_time * 1000000 / chunks));
		}
		if (set.verbose >= LOW) printf("\n");
	} else {
		sleep_time *= 1000000;
		usleep((unsigned int) sleep_time);
	}
}


/* Timestamp */
void timestamp(char *str) {
	struct timeval now;
	struct tm *t;

	gettimeofday(&now, NULL);
	t = localtime(&now.tv_sec);
	printf("[%02d/%02d %02d:%02d:%02d %s]\n", t->tm_mon + 1,
	       t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, str);
	return;
}


char *file_timestamp() {
	static char str[_BUFF_SIZE];
	struct timeval now;
	struct tm *t;

	gettimeofday(&now, NULL);
	t = localtime(&now.tv_sec);
	snprintf(str, sizeof(str), "%02d%02d_%02d:%02d:%02d", t->tm_mon + 1,
	         t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	return (str);
}


int checkPID(char *pidfile) {

	FILE *pidptr = NULL;
	pid_t rtgpoll_pid;

	rtgpoll_pid = getpid();
	if ((pidptr = fopen(pidfile, "r")) != NULL) {
		char temp_pid[_BUFF_SIZE];
		int verify = 0;
		/* rtgpoll appears to already be running. */
		while (fgets(temp_pid, _BUFF_SIZE, pidptr)) {
			verify = atoi(temp_pid);
			if (set.verbose >= LOW) {
				printf("Checking another instance with pid %d.\n", verify);
			}
			if (kill(verify, 0) == 0) {
				printf("rtgpoll is already running. Exiting.\n");
				exit(1);
			}
			else {
				/* This process isn't really running */
				if (set.verbose >= LOW) {
					printf("PID %d is no longer running. Starting anyway.\n",
					       verify);
				}
				unlink(pidfile);
			}
		}
	}

	/* This is good, rtgpoll is not running. */
	if ((pidptr = fopen(pidfile, "w")) != NULL) {
		fprintf(pidptr, "%d\n", rtgpoll_pid);
		fclose(pidptr);
		return (0);
	}
	else {
		/* Yuck, we can't even write the PID. Goodbye. */
		return (-1);
	}
}

int alldigits(char *s) {
	int result = TRUE;

	if (*s == '\0') return FALSE;

	while (*s != '\0') {
		if (!(*s >= '0' && *s <= '9'))
			return FALSE;
		s++;
	}
	return result;
}
