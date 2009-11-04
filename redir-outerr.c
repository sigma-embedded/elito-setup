/*	--*- c -*--
 * Copyright (C) 2009 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
	int		fd;
	if (argc<4) {
		write(2, "Usage: redir-outerr <OUT> <ERR> <CMD> <PARAMS>*\n", 48);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_WRONLY);
	if (fd==-1) {
		write(2, "redir-outerr: open(OUT) failed\n", 31);
		return EXIT_FAILURE;
	}

	if (fd!=1) {
		dup2(fd, 1);
		close(fd);
	}


	fd = open(argv[2], O_WRONLY);
	if (fd==-1) {
		write(2, "redir-outerr: open(ERR) failed\n", 31);
		return EXIT_FAILURE;
	}

	if (fd!=2) {
		dup2(fd, 2);
		close(fd);
	}

	execv(argv[3], argv+3);
	write(2, "redir-outerr: execv() failed\n", 27);
	return EXIT_FAILURE;
}
