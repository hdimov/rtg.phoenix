//
// Created by Hristo Hristov on 8/22/16.
//

#include "rtg.h"

#ifndef RTG_PHOENIX_PRLOGGER_H
#define RTG_PHOENIX_PRLOGGER_H

#include "psql.h"

extern MYSQL _mysql;
extern config_t set;

void* prlogger(void *_thread_args);

#endif //RTG_PHOENIX_PRLOGGER_H
