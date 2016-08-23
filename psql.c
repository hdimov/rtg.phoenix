
/****************************************************************************
   Program:     psql.c, v0.8.0
   Author(s):   ico.dimov
   Purpose:     RTG.phoenix MySQL/MariaDB/PostgreSQL functions;
****************************************************************************/

//
// Created by Hristo Hristov on 8/23/16.
//

#include "psql.h"

MYSQL _mysql;

/*

CREATE TABLE `zoom_In_1008` (
  `id` int(11) NOT NULL default '0',
  `dtime` datetime NOT NULL default '0000-00-00 00:00:00',
  `counter` bigint(20) NOT NULL default '0',
  KEY `id` (`id`,`dtime`)
) ENGINE=MyISAM DEFAULT CHARSET=cp1251
 
CREATE TABLE `tpl_rel_counter_store` (
  `id` int unsigned NOT NULL default '0',
  `dtime` datetime NOT NULL default '0000-00-00 00:00:00',
  `counter` bigint NOT NULL default '0',
  KEY `_k_01` using btree (`id`) ,
  KEY `_k_02` using btree (`dtime`)
) ENGINE=InnoDB DEFAULT CHARSET=cp1251

 */

int _db_create_counter_store_rel(MYSQL *_ptr_mysql, char *_rel, char *_initial_query) {
	
	char query[_BUFF_SIZE];
	
	snprintf(
		query,
		sizeof(query),
		"create table %s like tpl_rel_counter_store",
		_rel
	);

	int _dbq_status = mysql_query(_ptr_mysql, query);
	
	if (_dbq_status == 0)
		_dbq_status = mysql_query(_ptr_mysql, _initial_query);
	
//	sprintf(
//		_log_str,
//		"[%8s] database no: %d,  decr: %s (%s)",
//		"error",
//		mysql_errno(&_mysql),
//		mysql_error(&_mysql),
//		_entry -> table
//	);
//
//	log2me(DEBUG, _log_str);
	
	
}

int _db_query(MYSQL *_mysql, char *_query) {

//	if (set.verbose >= HIGH)
//		printf("SQL: %s\n", _query);
	
	if (mysql_query(_mysql, _query))
		return FALSE;

//		if (set.verbose >= LOW)
//			fprintf(stderr, "[  err] MySQL/MariaDB error: %s\n", mysql_error(_mysql));
	
	return TRUE;
	
}


int _db_connect(char *_database, MYSQL *_mysql) {

//	if (set.verbose >= LOW);
// fprintf(_fp_debug, "[ info] connecting to MySQL/MariaDB _database '%s' on '%s'...", _database, set.dbhost);
	
	mysql_init(_mysql);
	if (!mysql_real_connect(_mysql, set._db_host, set._db_user, set._db_pass, _database, 0, NULL, 0))
		return -1;
	// fprintf(_fp_debug, "[  err] failed to connect: %s\n", mysql_error(_mysql));
	
	return 0;
	
}

void _db_disconnect(MYSQL *_mysql) {
	
	mysql_close(_mysql);
	
}
