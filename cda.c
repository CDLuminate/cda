/* cda.c  ---  cd into Archive
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <archive.h>
#include <archive_entry.h>

#include "cda.h"
#include "lumin_log.h"

static int debug = 1; /* debug level, 1 for normal, 2 for detail */
static int silent= 0; /* 1 for silent, i.e. no progress bar */

/*
 * CDA functions
 */

static void
Usage (char *progname)
{	
	fprintf (stderr, "------------------------------------------------------------------------\n");
	fprintf (stderr, "  CDA %s -- built on %s %s \n", CDA_VERSION,
			__DATE__, __TIME__);
	fprintf (stderr, "   :: %s\n", archive_version_string());
	fprintf (stderr, "------------------------------------------------------------------------\n");
	fprintf (stderr, ""
"Synopsis:\n"
"    %s [options] ARCHIVE\n"
"Description:\n"
"    Extract archive into a temporary directory, and a shell will be\n"
"    be fired up for you. This temporary directory will be removed\n"
"    on exit of shell.\n"
"Options:\n"
"    -d <DIR>  Specify base path of temp directory.\n"
"              (would override the CDA env).\n"
"    -l        List archive components when extracting.\n"
"    -L        List archive components only.\n"
"    -X        Extract the archive and keep them, no shell.\n"
"    -v        Toggle debug (verbose) mode.\n"
"    -s        Toggle silence mode, no progress bar.\n"
"    -h        Show this help information.\n"
"Environment:\n"
"    CDA       Set temp dir to use.  (current: %s)\n"
"", progname,
	(NULL==getenv("CDA"))?("/tmp"):getenv("CDA"),
	(NULL==getenv("CDASH"))?("/bin/bash"):getenv("CDASH"));
	return;
}

static int remove_tmpdir (char * destdir, int force);
static int cda_fetchenv (char *** env, char ** prefix, char ** shell);
static int copy_data (struct archive *, struct archive *);
static int cda_archive_handler (struct archive *, int, const int);
static void cda_hook_exit (int status, void * arg);
static void cda_signal_handler (int signal);

int archfd;
off_t archfilesize;

int
main (int argc, char **argv, char **env)
{
	struct archive * arch; /* libarchive, for archive reading */

	int cda_action = CDA_EXTRACT | CDA_SHELL; /* default action */
	int flags; /* flag for libarchive */

	char * prefix = PREFIX; /* tmp dir path prefix */
	char * shell = cda_getshell(); /* default shell */
	char * archfname; /* archive file name */
	char   template[] = TEMPLATE; /* used by mkdtemp */
	char * temp_dir; /* store the temp dir name */

	/* malloc buffers, check NULL  */
	char * curdir   = (char *) Malloc (4096); /* current dir */
	char * destdir  = (char *) Malloc (4096); /* tmp dir */
	Getcwd (curdir, 4095);
	cda_fetchenv (&env, &prefix, &shell);

	/* Signal action */
	struct sigaction cda_signal_act;
	struct sigaction cda_signal_old_act;

	{ /* check and parse argument */
		/* check argc */
		if (2 > argc) {
			LOG_WARN ("Missing arguments.");
			Usage (argv[0]);
			exit (EXIT_FAILURE);
		}
		/* parse argument with getopt() */
		int opt;
		while ((opt = getopt(argc, argv, "d:lLXvhs")) != -1) {
			switch (opt) {
			case 'd': /* destination */
				prefix = optarg; /* this will override the env CDA */
				break;
			case 'l': /* call cda_list_archive */
				cda_action |= CDA_LIST;
				break;
			case 'L': /* list_only */
				cda_action = CDA_LIST;
				break;
			case 'X': /* extract only */
				cda_action = CDA_EXTRACT;
				break;
			case 'v': /* verbose */
				debug = 2;
				break;
			case 'h': /* dump help info */
				Usage (argv[0]);
				exit (EXIT_SUCCESS);
				break;
			case 's': /* toggle silent flag */
				silent = 1;
				break;
			default:
				LOG_ERROR ("option not defined.");
				exit (EXIT_FAILURE);
			}
		}
		if (optind >= argc) {
			LOG_ERROR ("no archive specified.");
			exit (EXIT_FAILURE);
		} else {
			archfname = argv[optind];
		}
	}
	{ /* Stat */
		/* stat the target (archive)file/dir */
		struct stat * stat_buf = Malloc (sizeof(struct stat));
		Stat (archfname, stat_buf);
		/* check whether target archive is a plain file */
		if (!( stat_buf->st_mode & S_IFREG )) {
			LOG_ERROR ("only plain files could be processed.");
			exit (EXIT_FAILURE);
		}
		archfilesize = stat_buf -> st_size;
		free (stat_buf);
	}
	{ /* Access -- check mode of target */
		Access (archfname, R_OK);
		if (1<debug) LOG_DEBUGF ("access(\"%s\", R_OK) success.", archfname);
	}
	{ /* init libarchive settings, and open archive file*/
		archfd = Open (archfname, O_RDONLY);
		posix_fadvise (archfd, 0, 0, POSIX_FADV_SEQUENTIAL);
		/* libarchive settings */
		flags  = ARCHIVE_EXTRACT_TIME;
		flags |= ARCHIVE_EXTRACT_PERM;
		flags |= ARCHIVE_EXTRACT_ACL;
		flags |= ARCHIVE_EXTRACT_FFLAGS;

		arch = archive_read_new ();
		archive_read_support_filter_all (arch);
		archive_read_support_format_all (arch);
		/* open archive */
		//if (ARCHIVE_OK != archive_read_open_filename (arch, archfname, 10240)) {
		if (ARCHIVE_OK != archive_read_open_fd (arch, archfd, 10240)) {
			LOG_ERRORF ("%s", archive_error_string(arch));
			exit (EXIT_FAILURE);
		}
	}
	{ /* create temporary directory */
		Chdir (prefix);
		temp_dir = Mkdtemp (template);
		if (1<debug) LOG_DEBUGF ("create temporary directory [%s/%s]", prefix, temp_dir);
		Chdir (temp_dir);
		Getcwd (destdir, 4095);
	}
	{ /* register on_exit hook, for removing the tmp dir */
		on_exit (cda_hook_exit, (void *)destdir);
		/* exit (1); for testing the hook */ 
	  /* prepare signal stuff */
		cda_signal_act.sa_handler = cda_signal_handler;
		cda_signal_act.sa_flags = SA_RESETHAND | SA_NODEFER;
		sigaction (SIGINT, &cda_signal_act, &cda_signal_old_act);
	}
	{ /* do the CDA matter with the forked child */
		LOG_INFOF ("Extracting Archive into [%s]...", destdir);
		if (cda_action & (CDA_EXTRACT|CDA_LIST)) {
			/* extract archive into temp_dir */
			pid_t pid = Fork ();
			if (0 == pid) { /* fork --> 0 : child */
				int child_ret = cda_archive_handler (arch, flags, cda_action);
				exit (child_ret);
			} else {        /* fork -/> 0 : parent */
				int status = 0;
				Waitpid (-1, &status, 0); // wait for any child
				if (0 != status) {
					LOG_ERRORF ("libarchive operations exited with error (%d).", status);
					exit (EXIT_FAILURE);
				}
				if (1>debug) LOG_DEBUGF ("libarchive operations are successful. (%d).", status);
			}
		}
	}
	{ /* fork and execve() a shell in the cda environment */
		if (cda_action & CDA_SHELL) {
			if (1<debug) LOG_DEBUGF ("fork and execve a shell for you, under [%s]", destdir);
			LOG_WARNF ("-*- \x1b[4mExit this shell when operations complete\x1b[24m -*-");
			int tmp;
			pid_t pid = Fork ();
			if (0 == pid) { /* child execve a shell */
				char * shellargv[] = { shell, NULL };
				execve (shell, shellargv, env);
			} else { /* partent wait */
				Waitpid (-1, &tmp, 0); /* sleep and wait for the shell */
			}
			/* when user exited bash above, this program continues from here */
		}
	}
	{ /* remove the temporary stuff */
		if (cda_action == CDA_EXTRACT) { /* extract only, i.e. keep */
			LOG_INFOF ("keeping temp directory [%s]", destdir);
		} else {
			if (1<debug) LOG_DEBUGF ("removing the temporary directory [%s]", destdir);
			remove_tmpdir (destdir, 1);
		}
	}
	{ /* free */
		free (curdir);
		free (destdir);
	}
	return 0;
}

static struct _pgbar {
	char ch;
	struct _pgbar * next;
} _xx; /* making the compiler happy */

struct _pgbar pgbar1;
struct _pgbar pgbar2;
struct _pgbar pgbar3;
struct _pgbar pgbar4;

struct _pgbar pgbar1 = { '-', &pgbar2 };
struct _pgbar pgbar2 = { '\\', &pgbar3 };
struct _pgbar pgbar3 = { '|', &pgbar4 };
struct _pgbar pgbar4 = { '/', &pgbar1 };

char
_cda_bar (void)
{
	(void) _xx; /* _xx is not used */
	static struct _pgbar * cur = &pgbar1;
	cur = cur -> next;
	return cur -> ch;
}

static int
cda_archive_handler (struct archive * arch, int flags, const int cda_action)
{
	struct archive * ext;
	struct archive_entry * entry;

	int r;
	char line_buf[4096] = {0};

	struct winsize w;
	ioctl (STDOUT_FILENO, TIOCGWINSZ, &w); /* get window size */

	ext = archive_write_disk_new ();
	archive_write_disk_set_options (ext, flags);
	archive_write_disk_set_standard_lookup (ext);

	if (!silent) fprintf (stdout, "\x1b[?25l"); /* hide cursor, see screen(1) */
	while (1) {

		r = archive_read_next_header (arch, &entry);
		if (ARCHIVE_EOF == r)
			break;
		if (ARCHIVE_OK > r)
			LOG_ERRORF ("%s", archive_error_string (arch));
		if (ARCHIVE_WARN > r)
			exit (EXIT_FAILURE);

		if (cda_action & CDA_LIST)
			fprintf (stdout, "%s\n", archive_entry_pathname (entry));

		if (!silent) { /* Progress indicator, borrowed some bit from Debian's APT */
			ioctl (STDOUT_FILENO, TIOCGWINSZ, &w); /* get window size */
			snprintf (line_buf, w.ws_col+26, 
					"\x1b[42m\x1b[30m[%c%3.2d%%]\x1b[49m\x1b[39m \x1b[32m%-*.*s\x1b[m", 
					_cda_bar(), 
					(int)(100*lseek (archfd, (off_t)0, SEEK_CUR)/archfilesize), 
					w.ws_col+21,
				   	w.ws_col+21,
				   	archive_entry_pathname (entry));
			fprintf (stdout, "\0337%s", line_buf); /* save cursor, and print string */
			fsync (STDOUT_FILENO);
			fprintf (stdout, "\0338"); /* restore cursor */
			fsync (STDOUT_FILENO);
			//usleep (20000); /* for debugging progress bar */
		}

		if (cda_action == CDA_LIST) {
			archive_read_data_skip (arch);
		} else {

			r = archive_write_header (ext, entry);
			if (ARCHIVE_OK > r)
				LOG_WARNF ("%s", archive_error_string (ext));
			else if (0 < archive_entry_size(entry)) {
				r = copy_data (arch, ext);
				if (ARCHIVE_OK > r)
					LOG_ERRORF ("%s", archive_error_string(ext));
				if (ARCHIVE_WARN > r)
					exit (EXIT_FAILURE);
			}

			r = archive_write_finish_entry (ext);
			if (ARCHIVE_OK > r)
				LOG_WARNF ("%s", archive_error_string(ext));
			if (ARCHIVE_WARN > r)
				exit (EXIT_FAILURE);
		}
	}
	if (!silent) {/* terminate progress indicator */
		fprintf (stdout, "\x1b[2K\r");
		fprintf (stdout, "\x1b[?25h"); /* unhive cursor, see screen(1) */
	}

	archive_read_close (arch);
	archive_read_close (ext);
	archive_read_free (arch);
	archive_read_free (ext);
	return EXIT_SUCCESS;
}

static int
copy_data (struct archive * ar, struct archive * aw)
{
	int r;
	const void * buff;
	size_t size;
	off_t offset;

	while (1) {
		r = archive_read_data_block (ar, &buff, &size, &offset);
		if (ARCHIVE_EOF == r)
			return (ARCHIVE_OK);
		if (ARCHIVE_OK > r)
			return r;
		
		r = archive_write_data_block (aw, buff, size, offset);
		if (ARCHIVE_OK > r) {
			fprintf (stderr, "%s\n", archive_error_string(aw));
			return r;
		}
	}
}

static int
cda_fetchenv (char *** env, char ** prefix, char ** shell)
{
	/* check env and apply env CDA, CDASH */
	if (NULL == getenv("CDA")) {
		if (1<debug) perror ("getenv");
	} else {
		*prefix = getenv("CDA");
		//if (debug) printf ("I: CDA = \"%s\"\n", *prefix);
	}
	if (NULL == getenv("CDASH")) {
		if (1<debug) perror ("getenv");
	} else {
		*shell = getenv("CDASH");
		//if (debug) printf ("I: CDASH = \"%s\"\n", *shell);
	}
	return 0;
}
	
static int
remove_tmpdir (char * destdir, int force)
{   
	int _tmp = 0;
	pid_t pid = Fork ();
	if (pid == 0) {  /* fork : child */
		/* construct newargv for rm */
		char * rmenv[]  = { NULL };
		char * rmargv[] = { "rm", (0==force)?("-ri"):("-rf"), destdir, NULL };
		{ /* dump the RM command line */
			if (1<debug) LOG_WARNF (" execve(): %s %s %s", rmargv[0], rmargv[1], rmargv[2]);
		}
		execve ("/bin/rm", rmargv, rmenv);
		LOG_ERRORF ("%s", strerror(errno));/* execve only returns on error */
		exit (EXIT_FAILURE);
	} else {  /* fork : parent */
		Waitpid (-1, &_tmp, 0);
		if (0 == _tmp) {
			LOG_INFOF ("Removal of [%s] (%d) : Success.", destdir, _tmp);
		} else {
			LOG_ERRORF ("Removal of [%s] (%d) : Failed.", destdir, _tmp);
			exit (EXIT_FAILURE);
		}
	}
	return _tmp;
}

/* int on_exit(void (*function)(int , void *), void *arg); */
static void cda_hook_exit (int status, void * arg)
{
	if (1<debug) LOG_DEBUGF ("hook status %d", status);
	/* on exit, we need to remove the temporary directory if it still exists. */
	if (status != EXIT_SUCCESS) {
		if (1<debug) LOG_DEBUGF ("detecting residual temporary directory ...");
		struct stat stat_buf_;
		stat ((char *) arg, &stat_buf_); /* note: don't use wrapper here ! */
		if ((stat_buf_.st_mode & S_IFMT) == S_IFDIR) {
			if (1<debug) LOG_DEBUGF ("remove tmpdir %s via on_exit hook", (char *)arg);
			remove_tmpdir ((char *)arg, 1);
		}
	} else {
		; /* nothing to do */
	}
	return;
}

static void
cda_signal_handler (int signal)
{
	switch (signal) {
	case SIGINT:
		/* we need to delete temporary directory on interruption */
		LOG_DEBUG ("Ctrl^C Triggered SIGINT handler");
		exit (EXIT_FAILURE);
		break;
	default:
		;
	}
	return;
}
