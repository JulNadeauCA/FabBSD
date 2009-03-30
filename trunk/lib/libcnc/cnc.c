/*	$FabBSD$	*/

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "pathnames.h"

char *cncErrorMsg;
int cncFd;

void
CNC_Init(void)
{
	cncErrorMsg = "";
	cncFd = -1;
}

void
CNC_Destroy(void)
{
}

const char *
CNC_GetError(void)
{
	return ((const char *)cncErrorMsg);
}

void
CNC_SetError(const char *fmt, ...)
{
	va_list args;
	char *buf;
	
	va_start(args, fmt);
	(void)vasprintf(&buf, fmt, args);
	va_end(args);
	Free(cncErrorMsg);
	cncErrorMsg = buf;
}

/* Open /dev/cnc */
int
CNC_Open(const char *devpath)
{
	const char *path = (devpath != NULL) ? devpath : _PATH_DEV_CNC;

	if ((cncFd = open(devpath, O_WRONLY)) == -1) {
		CNC_SetError(strerror(errno));
		return (-1);
	}
	return (cncFd);
}

void
CNC_Close(void)
{
	if (cncFd != -1) {
		close(cncFd);
		cncFd = -1;
	}
}
