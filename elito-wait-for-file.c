/*	--*- c -*--
 * Copyright (C) 2008 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <asm/types.h>
#include <sys/inotify.h>

int main(int argc, char *argv[])
{
	int		fd;
	int		watch_desc;
	bool		found = false;
	char const	*fullname = argv[1];
	char		*pathname = strdupa(fullname);
	//char const	*bname    = basename(pathname);
	char const	*dname    = dirname(pathname);
	struct timeval	to;

	if (argc<2)
		return EXIT_FAILURE;
		
	fd = inotify_init();
	
	if (fd<0) {
		perror("inotify_init()");
		return EXIT_FAILURE;
	}

	watch_desc = inotify_add_watch(fd, dname, IN_CREATE);
	if (watch_desc<0) {
		perror("inotify_add_watch()");
		close(fd);
		return EXIT_FAILURE;
	}

	to.tv_sec  = atoi(argv[2]);
	to.tv_usec = 0;

	for (found=false; !found;) {
		struct stat	st;
		fd_set		fds;
		int		rc;
		char		buf[1024];
		
		if (lstat(fullname, &st)==0) {
			found = true;
			continue;
		}
		
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		rc = select(fd+1, &fds, NULL, NULL, &to);
		if (rc<0) {
			perror("select()");
			close(fd);
			return EXIT_FAILURE;
		} else if (rc==0)
			break;		/* timeout */

		(void)read(fd, buf, sizeof buf);
	}

	close(fd);
	return found ? EXIT_SUCCESS : EXIT_FAILURE;
}
