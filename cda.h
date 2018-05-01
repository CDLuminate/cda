/*
Copyright (C) 2015 Lumin <cdluminate@gmail.com>
License: GPL-3.0+
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 .
 This package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
 .
 On Debian systems, the complete text of the GNU General
 Public License version 3 can be found in "/usr/share/common-licenses/GPL-3".
*/

/* cda.h -- project configuration and helper functions */

#ifndef CDA_H_
#define CDA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <archive.h>
#include <archive_entry.h>
#include "lumin_log.h"

/* Project configurations */
#define PREFIX         ("/tmp/")
#define TEMPLATE       ("cda.XXXXXX")
#define CDA_VERSION    ("1.8~wip (17 May. 2017)")
#define SHELL_FALLBACK ("/bin/bash")

/* Action flags */
#define CDA_LIST         (0x0001)
#define CDA_EXTRACT      (0x0010)
#define CDA_SHELL        (0x0100)

/*
 * Helper function
 */
char *
cda_getshell(void)
{
	char * shell = getenv("SHELL");
	if (NULL == shell) {
		shell = SHELL_FALLBACK;
	}
	return shell;
}

/*
 * Wrapper functions 
 */

inline static int
Open (const char *pathname, int flags) {
	int ret = open (pathname, flags);
	if (-1 == ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static char *
Getcwd (char * buf, size_t size) {
	char * ret = getcwd (buf, size);
	if (NULL == ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static int
Stat (char * pathname, struct stat * buf) {
	int ret = stat (pathname, buf);
	if (0 != ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static int
Access (char * pathname, int mode) {
	int ret = access (pathname, mode);
	if (0 != ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static char *
Mkdtemp (char * template) {
	char * ret = mkdtemp (template);
	if (NULL == ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static int
Chdir (const char *path) {
	int ret = chdir (path);
	if (0 != ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static pid_t
Fork (void) {
	pid_t ret = fork ();
	if (-1 == ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static pid_t
Waitpid (pid_t pid, int *status, int options) {
	pid_t ret = waitpid (pid, status, options);
	if (-1 == ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

inline static void *
Malloc (size_t size) {
	void * ret = malloc (size);
	if (NULL == ret) {
		LOG_ERRORF ("%s", strerror(errno));
		exit (EXIT_FAILURE);
	}
	return ret;
}

#endif /* CDA_H_ */
