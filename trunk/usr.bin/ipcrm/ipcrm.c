/*	$OpenBSD: ipcrm.c,v 1.10 2005/12/19 19:13:50 millert Exp $*/

/*
 * Copyright (c) 1994 Adam Glass
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Adam Glass BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include <signal.h>

#define IPC_TO_STR(x) (x == 'Q' ? "msq" : (x == 'M' ? "shm" : "sem"))
#define IPC_TO_STRING(x) (x == 'Q' ? "message queue" : \
	(x == 'M' ? "shared memory segment" : "semaphore"))

int signaled;

void	usage(void);
int	msgrm(key_t, int);
int	shmrm(key_t, int);
int	semrm(key_t, int);
void	not_configured(int);

void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-M shmkey] [-m shmid] [-Q msgkey]\n"
			"         [-q msqid] [-S semkey] [-s semid] ...\n",
		__progname);
	exit(1);
}

int
msgrm(key_t key, int id)
{
	if (key) {
		id = msgget(key, 0);
		if (id == -1)
			return (-1);
	}
	return (msgctl(id, IPC_RMID, NULL));
}

int
shmrm(key_t key, int id)
{
	if (key) {
		id = shmget(key, 0, 0);
		if (id == -1)
			return (-1);
	}
	return (shmctl(id, IPC_RMID, NULL));
}

int
semrm(key_t key, int id)
{
	union semun arg;

	if (key) {
		id = semget(key, 0, 0);
		if (id == -1)
			return (-1);
	}
	return (semctl(id, 0, IPC_RMID, arg));
}

/* ARGSUSED */
void
not_configured(int signo)
{
	signaled++;
}

int
main(int argc, char *argv[])
{
	int c, result, errflg, target_id;
	key_t target_key;

	errflg = 0;
	signal(SIGSYS, not_configured);
	while ((c = getopt(argc, argv, ":q:m:s:Q:M:S:")) != -1) {
		signaled = 0;
		switch (c) {
		case 'q':
		case 'm':
		case 's':
			target_id = atoi(optarg);
			if (c == 'q')
				result = msgrm(0, target_id);
			else if (c == 'm')
				result = shmrm(0, target_id);
			else
				result = semrm(0, target_id);
			if (result < 0) {
				errflg++;
				if (!signaled)
					warn("%sid(%d)",
					    IPC_TO_STR(toupper(c)), target_id);
				else
					warnx("%ss are not configured in the running kernel",
					    IPC_TO_STRING(toupper(c)));
			}
			break;
		case 'Q':
		case 'M':
		case 'S':
			target_key = atol(optarg);
			if (target_key == IPC_PRIVATE) {
				warnx("can't remove private %ss", IPC_TO_STRING(c));
				continue;
			}
			if (c == 'Q')
				result = msgrm(target_key, 0);
			else if (c == 'M')
				result = shmrm(target_key, 0);
			else
				result = semrm(target_key, 0);
			if (result < 0) {
				errflg++;
				if (!signaled)
					warn("%skey(%ld)", IPC_TO_STR(c),
					    target_key);
				else
					warnx("%ss are not configured in the running kernel",
					    IPC_TO_STRING(c));
			}
			break;
		case ':':
			warnx("option -%c requires an argument", optopt);
			usage();
		default:
			warnx("unrecognized option: -%c", optopt);
			usage();
		}
	}

	if (optind != argc) {
		warnx("unknown argument: %s", argv[optind]);
		usage();
	}
	exit(errflg);
}
