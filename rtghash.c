
/****************************************************************************
   Program:     rtghash.c, v0.7.4-r1
   Author(s):   rbeverly, hdimov
   Purpose:     RTG target hash table routines
****************************************************************************/

#include "rtg.h"

/* Initialize hash table */
void init_hash() {

	int i;

	for (i = 0; i < _HASH_SIZE; i++)
		hash.table[i] = NULL;

	if (set.verbose >= LOW) {

		printf(
			"[ info] initialize hash table pointers: %d bytes.\n",
			(unsigned int) (_HASH_SIZE * sizeof(target_t *))
		);

	}

}


void init_hash_walk() {
	hash.bucket = 0;
	hash.target = hash.table[hash.bucket];
}

target_t *getNext() {
	target_t *next = NULL;

	while (!(hash.target)) {
		(hash.bucket)++;
		if (hash.bucket >= _HASH_SIZE)
			return NULL;
		hash.target = hash.table[hash.bucket];
	}
	next = hash.target;
	hash.target = (hash.target)->next;
	return next;
}


/* Free hash table memory */
void free_hash() {
	target_t *ptr = NULL;
	target_t *f = NULL;
	int i;

	for (i = 0; i < _HASH_SIZE; i++) {
		ptr = hash.table[i];
		while (ptr) {
			f = ptr;
			ptr = ptr->next;
			free(f);
		}
	}
}


/* Hash the target */
unsigned long make_key(const void *entry) {

	const target_t *t = (const target_t *) entry;
	const char *h = (const char *) t->host;
	const char *o = (const char *) t->objoid;
	unsigned long hashval;

	if (!h) return 0;

	for (hashval = 0; *o != '\0'; o++)
		hashval = (((unsigned int) *o) | 0x20) + 31 * hashval;

	for (; *h != '\0'; h++)
		hashval = (((unsigned int) *h) | 0x20) + 31 * hashval;

	return (hashval % _HASH_SIZE);

}


/* Set state of all targets to state - used on init, update targets, etc */
void mark_targets(int state) {
	target_t *p = NULL;
	int i = 0;

	for (i = 0; i < _HASH_SIZE; i++) {
		p = hash.table[i];
		while (p) {
			p->init = state;
			p = p->next;
		}
	}
}


/* Delete targets marked with state = state */
int delete_targets(int state) {
	target_t *p = NULL;
	target_t *d = NULL;
	int i = 0;
	int entries = 0;

	for (i = 0; i < _HASH_SIZE; i++) {
		p = hash.table[i];
		while (p) {
			d = p;
			p = p->next;
			if (d->init == state) {
				del_hash_entry(d);
				entries++;
			}
		}
	}
	return entries;
}


/* Dump the entire target hash [sequential num/bucket number] */
void walk_target_hash() {
	target_t *p = NULL;
	int targets = 0;
	int i = 0;
	
	printf("Dumping Target List:\n");
	for (i = 0; i < _HASH_SIZE; i++) {
		p = hash.table[i];
		while (p) {
			printf("[%d/%d]: %s %s %d %s %s %d\n", targets, i, p->host,
			       p->objoid, p->bits, p->community, p->table,
			       p->iid);
			targets++;
			p = p->next;
		}
	}
	printf("Total of %u targets [%u bytes of memory].\n",
	       targets, targets * sizeof(target_t));
}


/* return ptr to target if the entry exists in the named list */
void *in_hash(target_t *entry, target_t *list) {
	while (list) {
		if (compare_targets(entry, list)) {
			if (set.verbose >= HIGH) {
				printf("Found existing %s %s %s %d\n", list->host,
				       list->objoid, list->table, list->iid);
			}
			return list;
		}
		list = list->next;
	}
	return NULL;
}


/* TRUE if target 1 == target 2 */
int compare_targets(target_t *t1, target_t *t2) {
	if (!strcmp(t1->host, t2->host) &&
	    !strcmp(t1->objoid, t2->objoid) &&
	    !strcmp(t1->table, t2->table) &&
	    !strcmp(t1->community, t2->community) &&
	    (t1->iid == t2->iid))
		return TRUE;
	return FALSE;
}


/* Remove an item from the list.  Returns TRUE if successful. */
int del_hash_entry(target_t *new) {
	target_t *p = NULL;
	target_t *prev = NULL;
	unsigned int key;

	key = make_key(new);
	if (!in_hash(new, hash.table[key]))
		return FALSE;
	
	p = hash.table[key];
	/* assert(p != NULL) */
	while (p) {
		if (compare_targets(p, new)) {
			/* if successor, we need to point predecessor to it (if exists) */
			if (p->next) {
				/* there is a predecessor */
				if (prev)
					prev->next = p->next;
					/* there is no predecessor */
				else
					hash.table[key] = p->next;
			}
				/* no successor, delete just this target */
			else {
				if (prev)
					prev->next = NULL;
				else
					hash.table[key] = NULL;
			}
			free(p);
			return TRUE;
		}
		prev = p;
		p = p->next;
	}
	return FALSE;
}


/* Add an entry to hash if it is unique, otherwise free() it */
int add_hash_entry(target_t *new) {
	target_t *p = NULL;
	unsigned int key;

	key = make_key(new);
	p = hash.table[key];

	p = in_hash(new, hash.table[key]);
	if (p) {
		p->init = LIVE;
		free(new);
		return FALSE;
	}

	if (!hash.table[key]) {
		hash.table[key] = new;
	} else {
		p = hash.table[key];
		while (p->next) p = p->next;
		p->next = new;
	}
	return TRUE;
}


/*
Read a target file into a target_t hash table.  Our hash algorithm
roughly randomizes targets, easing SNMP load on end devices during
polling.  hash_target_file() can be called again to update the target
hash.  If hash_target_file() finds new target entries in the file, it
adds them to the hash.  If hash_target_file() finds entries in hash
but not in file, it removes said entries from hash.
*/

int hash_target_file(char *file) {

	FILE *fp;

	target_t *new = NULL;

	char buffer[_BUFF_SIZE];
	char _log_str[_BUFF_SIZE];

	char maxspeed[30];
	int entries = 0;
	int removed = 0;

	sprintf(
		_log_str,
		"[%8s] function BEGIN: %s",
		"info",
		__FUNCTION__
	);
	log2me(DEVELOP, _log_str);

	/* Open the target file */
	if ((fp = fopen(file, "r")) == NULL) {

		sprintf(
			_log_str,
			"[%8s] could not open targets file for reading: %s",
			"error",
			file
		);
		log2me(LOW, _log_str);

		return (-1);

	}

//	if (set.verbose >= LOW)
//		printf("\nReading RTG target list [%s].\n", file);

	sprintf(
		_log_str,
		"[%8s] reading RTG.phoenix target list from file: %s",
		"info",
		file
	);
	log2me(LOW, _log_str);

	mark_targets(STALE);

	/* Read each unique target into hash table */
	while (!feof(fp)) {

		fgets(buffer, _BUFF_SIZE, fp);

		if (!feof(fp) && buffer[0] != '#' && buffer[0] != ' ' && buffer[0] != '\n') {

			new = (target_t *) malloc(sizeof(target_t));

			if (!new) {
				printf("Fatal target malloc error!\n");
				exit(-1);
			}

			sscanf(
				buffer,
				"%64s %128s %hu %64s %64s %d %64s %30s",
				new->host,
				new->objoid,
				&(new->bits),
				new->community,
				new->table,
				&(new->iid),
				new->iface,
				maxspeed
			);

			if (alldigits(maxspeed)) {
//#ifdef HAVE_STRTOLL
				new->maxspeed = strtoll(maxspeed, NULL, 0);
//#else
//				new->maxspeed = strtol(maxspeed, NULL, 0);
//#endif
			} else {

				new->maxspeed = set.out_of_range;

			}

			sprintf(
				_log_str,
				"[%8s] host+oid: %s+%s, bits: %d, community: %s, table: %s, iid: %d, iface: %s, maxspeed: %lld",
				"info",
				new->host,
				new->objoid,
				new->bits,
				new->community,
				new->table,
				new->iid,
				new->iface,
				new->maxspeed
			);
			log2me(DEVELOP, _log_str);

//			if (set.verbose > DEBUG)
//				printf("Host[OID][OutOfRange]:%s[%s][%lld]\n",
//				       new->host, new->objoid, new->maxspeed);

			new->init = NEW;
			new->last_value = 0;
			new->next = NULL;
			entries += add_hash_entry(new);

		}

	}
	fclose(fp);
	removed = delete_targets(STALE);

	sprintf(
		_log_str,
		"[%8s] successfully hashed %d NEW targets, size taken: %d bytes.",
		"info",
		entries,
		entries * sizeof(target_t)
	);
	log2me(LOW, _log_str);

	if (removed > 0) {

		sprintf(
			_log_str,
			"[%8s] also removed %d STALE targets from hash.",
			"info",
			removed
		);
		log2me(LOW, _log_str);

	}

	sprintf(
		_log_str,
		"[%8s] function END: %s",
		"info",
		__FUNCTION__
	);
	log2me(DEVELOP, _log_str);

	return (entries);

}
