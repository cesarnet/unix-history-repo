/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
 *
 * %sccs.include.redist.c%
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)main.c	5.2 (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <fcntl.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"

typedef struct cmd_table {
	char *cmd;
	int nargs;
	int rconv;
	void (*func) __P((DB *, char **));
	char *usage, *descrip;
} cmd_table;

int stopstop;
DB *globaldb;

void bstat	__P((DB *, char **));
void cursor	__P((DB *, char **));
void delcur	__P((DB *, char **));
void delete	__P((DB *, char **));
void dump	__P((DB *, char **));
void first	__P((DB *, char **));
void get	__P((DB *, char **));
void help	__P((DB *, char **));
void iafter	__P((DB *, char **));
void ibefore	__P((DB *, char **));
void insert	__P((DB *, char **));
void keydata	__P((DBT *, DBT *));
void last	__P((DB *, char **));
void list	__P((DB *, char **));
void load	__P((DB *, char **));
void mstat	__P((DB *, char **));
void next	__P((DB *, char **));
int  parse	__P((char *, char **, int));
void previous	__P((DB *, char **));
void show	__P((DB *, char **));
void usage	__P((void));
void user	__P((DB *));

cmd_table commands[] = {
	"?",	0, 0, help, "help", NULL,
	"b",	0, 0, bstat, "bstat", "stat btree",
	"c",	1, 1, cursor,  "cursor word", "move cursor to word",
	"delc",	0, 0, delcur, "delcur", "delete key the cursor references",
	"dele",	1, 1, delete, "delete word", "delete word",
	"d",	0, 0, dump, "dump", "dump database",
	"f",	0, 0, first, "first", "move cursor to first record",
	"g",	1, 1, get, "get word", "locate word",
	"h",	0, 0, help, "help", "print command summary",
	"ia",	2, 1, iafter, "iafter key data", "insert data after key",
	"ib",	2, 1, ibefore, "ibefore key data", "insert data before key",
	"in",	2, 1, insert, "insert word def", "insert key with data def",
	"la",	0, 0, last, "last", "move cursor to last record",
	"li",	1, 1, list, "list file", "list to a file",
	"loa",	1, 1, load, "load file", NULL,
	"loc",	1, 1, get, "get word", NULL,
	"m",	0, 0, mstat, "mstat", "stat memory pool",
	"n",	0, 0, next, "next", "move cursor forward one record",
	"p",	0, 0, previous, "previous", "move cursor back one record",
	"q",	0, 0, NULL, "quit", "quit",
	"sh",	1, 0, show, "show page", "dump a page",
	{ NULL },
};

int recno;					/* use record numbers */
char *dict = "words";				/* default dictionary */
char *progname;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	DB *db;
	BTREEINFO b;

	progname = *argv;

	b.flags = 0;
	b.cachesize = 0;
	b.maxkeypage = 0;
	b.minkeypage = 0;
	b.psize = 0;
	b.compare = NULL;
	b.prefix = NULL;
	b.lorder = 0;

	while ((c = getopt(argc, argv, "bc:di:lp:ru")) != EOF) {
		switch (c) {
		case 'b':
			b.lorder = BIG_ENDIAN;
			break;
		case 'c':
			b.cachesize = atoi(optarg);
			break;
		case 'd':
			b.flags |= R_DUP;
			break;
		case 'i':
			dict = optarg;
			break;
		case 'l':
			b.lorder = LITTLE_ENDIAN;
			break;
		case 'p':
			b.psize = atoi(optarg);
			break;
		case 'r':
			recno = 1;
			break;
		case 'u':
			b.flags = 0;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (recno)
		db = dbopen(*argv == NULL ? NULL : *argv, O_RDWR,
		    0, DB_RECNO, NULL);
	else
		db = dbopen(*argv == NULL ? NULL : *argv, O_CREAT|O_RDWR,
		    0600, DB_BTREE, &b);

	if (db == NULL) {
		(void)fprintf(stderr, "dbopen: %s\n", strerror(errno));
		exit(1);
	}
	globaldb = db;
	user(db);
	exit(0);
	/* NOTREACHED */
}

void
user(db)
	DB *db;
{
	FILE *ifp;
	int argc, i, last;
	char *lbuf, *argv[4], buf[512];

	if ((ifp = fopen("/dev/tty", "r")) == NULL) {
		(void)fprintf(stderr,
		    "/dev/tty: %s\n", strerror(errno));
		exit(1);
	}
	for (last = 0;;) {
		(void)printf("> ");
		(void)fflush(stdout);
		if ((lbuf = fgets(&buf[0], 512, ifp)) == NULL)
			break;
		if (lbuf[0] == '\n') {
			i = last;
			goto uselast;
		}
		lbuf[strlen(lbuf) - 1] = '\0';

		if (lbuf[0] == 'q')
			break;

		argc = parse(lbuf, &argv[0], 3);
		if (argc == 0)
			continue;

		for (i = 0; commands[i].cmd != NULL; i++)
			if (strncmp(commands[i].cmd, argv[0],
			    strlen(commands[i].cmd)) == 0)
				break;

		if (commands[i].cmd == NULL) {
			(void)fprintf(stderr,
			    "%s: command unknown ('help' for help)\n", lbuf);
			continue;
		}

		if (commands[i].nargs != argc - 1) {
			(void)fprintf(stderr, "usage: %s\n", commands[i].usage);
			continue;
		}

		if (recno && commands[i].rconv) {
			static recno_t nlong;
			nlong = atoi(argv[1]);
			argv[1] = (char *)&nlong;
		}
uselast:	last = i;
		(*commands[i].func)(db, argv);
	}
	if ((db->sync)(db) == RET_ERROR)
		perror("dbsync");
	else if ((db->close)(db) == RET_ERROR)
		perror("dbclose");
}

int
parse(lbuf, argv, maxargc)
	char *lbuf, **argv;
	int maxargc;
{
	int argc = 0;
	char *c;

	c = lbuf;
	while (isspace(*c))
		c++;
	while (*c != '\0' && argc < maxargc) {
		*argv++ = c;
		argc++;
		while (!isspace(*c) && *c != '\0') {
			c++;
		}
		while (isspace(*c))
			*c++ = '\0';
	}
	return (argc);
}

void
cursor(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;
	status = (*db->seq)(db, &key, &data, R_CURSOR);
	switch (status) {
	case RET_ERROR:
		perror("cursor");
		break;
	case RET_SPECIAL:
		(void)printf("key not found\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
delcur(db, argv)
	DB *db;
	char **argv;
{
	int status;

	status = (*db->del)(db, NULL, R_CURSOR);

	if (status == RET_ERROR)
		perror("delcur");
}

void
delete(db, argv)
	DB *db;
	char **argv;
{
	DBT key;
	int status;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;

	status = (*db->del)(db, &key, 0);
	switch (status) {
	case RET_ERROR:
		perror("delete");
		break;
	case RET_SPECIAL:
		(void)printf("key not found\n");
		break;
	case RET_SUCCESS:
		break;
	}
}

void
dump(db, argv)
	DB *db;
	char **argv;
{
	__bt_dump(db);
}

void
first(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_FIRST);

	switch (status) {
	case RET_ERROR:
		perror("first");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
get(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;

	status = (*db->get)(db, &key, &data, 0);

	switch (status) {
	case RET_ERROR:
		perror("get");
		break;
	case RET_SPECIAL:
		(void)printf("key not found\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
help(db, argv)
	DB *db;
	char **argv;
{
	int i;

	for (i = 0; commands[i].cmd; i++)
		if (commands[i].descrip)
			(void)printf("%s: %s\n",
			    commands[i].usage, commands[i].descrip);
}

void
iafter(db, argv)
	DB *db;
	char **argv;
{
	DBT key, data;
	int status;

	if (!recno) {
		(void)fprintf(stderr,
		    "iafter only available for recno db's.\n");
		return;
	}
	key.data = argv[1];
	key.size = sizeof(recno_t);
	data.data = argv[2];
	data.size = strlen(data.data);
	status = (db->put)(db, &key, &data, R_IAFTER);
	switch (status) {
	case RET_ERROR:
		perror("iafter");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
ibefore(db, argv)
	DB *db;
	char **argv;
{
	DBT key, data;
	int status;

	if (!recno) {
		(void)fprintf(stderr,
		    "ibefore only available for recno db's.\n");
		return;
	}
	key.data = argv[1];
	key.size = sizeof(recno_t);
	data.data = argv[2];
	data.size = strlen(data.data);
	status = (db->put)(db, &key, &data, R_IBEFORE);
	switch (status) {
	case RET_ERROR:
		perror("ibefore");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
insert(db, argv)
	DB *db;
	char **argv;
{
	int status;
	DBT data, key;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;
	data.data = argv[2];
	data.size = strlen(argv[2]) + 1;

	status = (*db->put)(db, &key, &data, R_NOOVERWRITE);
	switch (status) {
	case RET_ERROR:
		perror("put");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
last(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_LAST);

	switch (status) {
	case RET_ERROR:
		perror("last");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
list(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	FILE *fp;
	int status;

	if ((fp = fopen(argv[1], "w")) == NULL) {
		(void)fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return;
	}
	status = (*db->seq)(db, &key, &data, R_FIRST);
	while (status == RET_SUCCESS) {
		(void)fprintf(fp, "%s\n", key.data);
		status = (*db->seq)(db, &key, &data, R_NEXT);
	}
	if (status == RET_ERROR)
		perror("list");
}

void
load(db, argv)
	DB *db;
	char **argv;
{
	register char *p, *t;
	FILE *fp;
	DBT data, key;
	int status;
	char b1[256], b2[256];

	if ((fp = fopen(argv[1], "r")) == NULL) {
		perror(argv[1]);
		return;
	}
	(void)printf("loading %s...\n", dict);

	key.data = b1;
	data.data = b2;
	while (fgets(b1, sizeof(b1), fp) != NULL) {
		data.size = strlen(b1);
		b1[data.size - 1] = '\0';
		for (p = &b1[data.size - 2], t = b2; p >= b1; *t++ = *p--);
		b2[data.size - 1] = '\0';
		key.size = data.size;

		status = (*db->put)(db, &key, &data, R_NOOVERWRITE);
		switch (status) {
		case RET_ERROR:
			perror("load/put");
			exit(1);
		case RET_SPECIAL:
			(void)fprintf(stderr, "duplicate: %s\n", key.data);
			exit(1);
		case RET_SUCCESS:
			break;
		}
	}
	(void)fclose(fp);
}

void
next(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_NEXT);

	switch (status) {
	case RET_ERROR:
		perror("next");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
previous(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_PREV);

	switch (status) {
	case RET_ERROR:
		perror("previous");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
show(db, argv)
	DB *db;
	char **argv;
{
	BTREE *t;
	PAGE *h;
	pgno_t pg;

	pg = atoi(argv[1]);
	if (pg == 0) {
		(void)printf("page 0 is meta-data page.\n");
		return;
	}

	t = db->internal;
	if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL) {
		(void)printf("getpage of %ld failed\n", pg);
		return;
	}
	__bt_dpage(h);
	mpool_put(t->bt_mp, h, 0);
}

void
bstat(db, argv)
	DB *db;
	char **argv;
{
	(void)printf("BTREE\n");
	__bt_stat(db);
}

void
mstat(db, argv)
	DB *db;
	char **argv;
{
	(void)printf("MPOOL\n");
	mpool_stat(((BTREE *)db->internal)->bt_mp);
}

void
keydata(key, data)
	DBT *key, *data;
{
	if (!recno && key->size > 0)
		(void)printf("%s/", key->data);
	if (data->size > 0)
		(void)printf("%s", data->data);
	(void)printf("\n");
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-bdlu] [-c cache] [-i file] [-p page] [file]\n",
	    progname);
	exit (1);
}
