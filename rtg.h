/****************************************************************************
   Program:     $Id: rtg.h,v 1.20 2003/09/25 15:56:04 rbeverly Exp $ 
   Author:      $Author: rbeverly $
   Date:        $Date: 2003/09/25 15:56:04 $
   Description: RTG headers
****************************************************************************/

#ifndef _RTG_H_
#define _RTG_H_ 1

// POSIX routines we'll use.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <stdint.h>


#include <netinet/in.h>

#include <sys/time.h>
#include <time.h>

#include <pthread.h>

#include <signal.h>

#include <mysql.h>

// init_snmp() must live here.
#include <net-snmp-config.h>
#include <net-snmp-includes.h>

#include "pqueue.h"
//#include "prlogger.h"

/* global TRUE/FALSE definitions */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE !FALSE
#endif

#define _DB_BACKEND_DISABLED 1

/* global constants */

#define _MAX_THREADS 992

#define _BUFF_SIZE 512
#define _BITS_IN_BYTE 8

#define _THIRTY_TWO 4294967295ul
#define _SIXTY_FOUR 18446744073709551615ull
/*
 *
 * Define CONFIG_PATHS places to search for the rtg.conf file.
 *
 * Note: RTG_HOME, was determined during autoconf ...
 *
 * */

// 1z0, temp re-definitions
#define RTG_HOME "/Users/lisp/ClionProjects/rtg.phoenix"
#define RTG_VERSION "0.7.4.phoenix-r3"

#define RTG_NAME "rtg.phoenix"
#define RTG_NAME_POLLER "rtg.phoenix.poller"

#define CONFIG_PATHS 3
#define CONFIG_PATH_1 ""
#define CONFIG_PATH_2 "/Users/lisp/ClionProjects/rtg.phoenix"

/* Defaults */
#define DEFAULT_CONF_FILE "rtg.phoenix.conf"

#define DEFAULT_THREADS 5
#define DEFAULT_INTERVAL 300

#define DEFAULT_HIGH_SKEW_SLOP 3
#define DEFAULT_LOW_SKEW_SLOP .5

// ?!
#define DEFAULT_OUT_OF_RANGE 93750000000ul
// ull ?

#define DEFAULT_DB_HOST "localhost"

#define DEFAULT_DB_DB "_rtg_phoenix"
#define DEFAULT_DB_USER "_db_user"
#define DEFAULT_DB_PASS "_db_pass"

#define DEFAULT_SNMP_VER 2 // which stands for snmp v2c
#define DEFAULT_SNMP_PORT 161

/* PID File */
#define PID_FILE "/tmp/rtgpoll.pid"

#define STAT_DESCRIP_ERROR 99

// #define HASH_SIZE 5000
// #define HASH_SIZE 10240
#define _HASH_SIZE 20480

/* pthread error messages */
#define PML_ERR "pthread_mutex_lock error\n"
#define PMU_ERR "pthread_mutex_unlock error\n"
#define PCW_ERR "pthread_cond_wait error\n"
#define PCB_ERR "pthread_cond_broadcast error\n"

/* pthread macros */
#define PT_MUTEX_LOCK(x) if (pthread_mutex_lock(x) != 0) printf(PML_ERR);
#define PT_MUTEX_UNLOCK(x) if (pthread_mutex_unlock(x) != 0) printf(PMU_ERR);
#define PT_COND_WAIT(x, y) if (pthread_cond_wait(x, y) != 0) printf(PCW_ERR);
#define PT_COND_BROAD(x) if (pthread_cond_broadcast(x) != 0) printf(PCB_ERR);

/* Verbosity levels LOW=info HIGH=info+SQL DEBUG=info+SQL+junk, DEVELOP everything */

// 0, 1, 2, 3, 4
enum debugLevel {
	OFF,
	LOW,
	HIGH,
	DEBUG,
	DEVELOP
};

enum dbType {
	__MYSQL,
	__PGSQL
};

/* Target state */
enum targetState {

	NEW, LIVE, STALE

};

/* Typedefs */
typedef struct worker_struct {

	int index;
	pthread_t thread;
	struct crew_struct *crew;

} worker_t;

typedef struct config_struct {
	
	enum dbType _db_type;
	
	char _db_host[64];
	char _db_name[64];
	char _db_user[64];
	char _db_pass[64];

	unsigned int interval;
	unsigned long long out_of_range;

	enum debugLevel verbose;

	unsigned short with_zeros;
	
	unsigned short db_off;
	
	unsigned short multiple;
	
	unsigned short snmp_ver;
	unsigned short snmp_port;

	unsigned int threads;

	float high_skew_slop;
	float low_skew_slop;

} config_t;

typedef struct target_struct {

	char host[64];
	char objoid[128];

	// NOTE: bits are set to 0, we interpret it as gauge values;
	unsigned short bits;

	char community[64];
	char table[64];

	unsigned int iid;

	char iface[64];
	
// NOTE: always demand for STRTOLL

// #ifdef HAVE_STRTOLL
//#else
//    long maxspeed;
//#endif

	// NOTE: why this is signed?
	long long maxspeed;
	
	enum targetState init;
	
	unsigned long long last_value;
	int last_status_sess;
	int last_status_snmp;
	
	// NOTE: adding this for future use;
	unsigned long long prev_value;
	
	unsigned long long insert_value;
	
	// also make necessary changes in order to log RTT time of last target(s) poll;

	// system time the request was sent;
	long _ts1_tv_sec;
	int _ts1_tv_usec;

	// system time the answer was received;
	long _ts2_tv_sec;
	int _ts2_tv_usec;

	struct target_struct *next;

} target_t;

typedef struct crew_struct {

	worker_t member[_MAX_THREADS];
	pthread_mutex_t mutex;

	// sending done, a signal for our main control thread;
	int _send_work_count;
	
	int _send_worker_count;
	pthread_cond_t _sending_done;

	// receiving done, a signal for our main thread;
	// int _recv_work_count;
	// pthread_cond_t _recv_done;

	// our receiving thread will also wait on go condition;
	// a start stignal for all worker threads;
	pthread_cond_t go;

	// pthread_cond_t _go_send;
	// pthread_cond_t _go_recv;

} crew_t;

typedef struct poll_stats {

	pthread_mutex_t mutex;
	// prlogger will wait on that;
	pthread_cond_t go;

	unsigned long long polls;
	// unsigned long long db_inserts;

	unsigned int round;
	unsigned int wraps;
	unsigned int no_resp;
	unsigned int out_of_range;
	unsigned int errors;
	unsigned int slow;
	
	// db/log errors?
	// ?! unsigned int timeout;

	double poll_time;
	
	pthread_t logger_thread;
	queue_t* _q_result;

} stats_t;

typedef struct hash_struct {

	target_t *table[_HASH_SIZE];
	int bucket;
	target_t *target;

} hash_t;

// NOTE: probably its a good idea to have per device hash table;
// only one problem appears; how to determine whether device is
// there or not;
// 'll continue with mysql insert queues;

typedef struct _device_hash_struct {
	
	target_t *table[_HASH_SIZE];
	int bucket;
	target_t *target;
	
} device_hash_t;

/*
 *
 * precasts: rtgpoll.c
 *
 * */
void *sig_handler(void *);

void usage(char *);

/* Precasts: rtgpoll.c */
void *poller(void *);
void *poller2(void *);

// void* sync_poller(void *thread_args);
// void *async_reader(void *thread_args);
void* sync_poller_v2(void *thread_args);

/* Precasts: rtgmysql.c */
int _db_insert(char *, MYSQL *);

int _db_connect(char *, MYSQL *);

void _db_disconnect(MYSQL *);

/* Precasts: rtgutil.c */
int read_rtg_config(char *, config_t *);

int write_rtg_config(char *, config_t *);

void config_defaults(config_t *);

void print_stats(stats_t);

void log_poll_stats(enum debugLevel verbose, stats_t stats);
void log_step_message(enum debugLevel verbose, char* msg);


void sleepy(float);

void timestamp(char *);
void ts2(char *);
void log2me(enum debugLevel verbose,  char *str);

int checkPID(char *);

int alldigits(char *);

/* Precasts: rtghash.c */
void init_hash();

void init_hash_walk();

target_t *getNext();

void free_hash();

unsigned long make_key(const void *);

void mark_targets(int);

int delete_targets(int);

void walk_target_hash();

void *in_hash(target_t *, target_t *);

int compare_targets(target_t *, target_t *);

int del_hash_entry(target_t *);

int add_hash_entry(target_t *);

int hash_target_file(char *);

// mix(ed) queue subs;
target_t* _target_dup(target_t* _src);

void* prlogger(void *_thread_args);

/*
 *
 * global data structures
 *
 * */

config_t set;
hash_t hash;

/* global variables */
int lock;
int waiting;

/*
 *
 * ?! char config_paths[CONFIG_PATHS][_BUFF_SIZE];
 *
 */

#endif /* not _RTG_H_ */
