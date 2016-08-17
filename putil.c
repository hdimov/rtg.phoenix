
/****************************************************************************
   Program:     putil.c, v0.7.4-r1
   Author(s):   hdimov
   Purpose:     RTG.Phoenix utility toolbox.
****************************************************************************/


//
// Created by Hristo Hristov on 7/9/16.
//

#include "putil.h"

void ts2(char *str) {

	struct timeval now;
	struct tm *t;

	gettimeofday(&now, NULL);
	t = localtime(&now.tv_sec);

	fprintf(
		stdout,
		"[%04d-%02d-%02d %02d:%02d:%02d.%06d] %s\n",
		t -> tm_year + 1900,
		t -> tm_mon + 1,
		t -> tm_mday,
		t -> tm_hour,
		t -> tm_min,
		t -> tm_sec,
		now.tv_usec,
		str
	);

	return;
}

void log2me(enum debugLevel verbose,  char *str) {

	if (set.verbose >= verbose)
		ts2(str);

}
