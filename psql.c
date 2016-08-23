/****************************************************************************
   Program:     rtgmysql.c, v0.0.1
   Author(s):   rbeverly, ico.dimov
   Purpose:     RTG.phoenix MySQL/MariaDB routines
****************************************************************************/

//
// Created by Hristo Hristov on 8/23/16.
//

#include "psql.h"

// extern FILE *_fp_debug;

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
