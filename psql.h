
/****************************************************************************
   Program:     psql.c, v0.8.0
   Author(s):   ico.dimov
   Purpose:     RTG.phoenix MySQL/MariaDB/PostgreSQL functions;
****************************************************************************/

//
// Created by Hristo Hristov on 8/23/16.
//

#include "rtg.h"

#ifndef RTG_PHOENIX_PSQL_H
#define RTG_PHOENIX_PSQL_H

#include <mysql.h>

extern config_t set;

int _db_create_counter_store_rel(MYSQL *_ptr_mysql, char *_rel, char *_initial_query);
#endif //RTG_PHOENIX_PSQL_H
