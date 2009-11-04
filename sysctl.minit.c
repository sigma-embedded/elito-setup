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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#define WRITE_MSG(FD,X)	(void)(write(FD, X, sizeof(X)-1))

int main(int argc, char *argv[])
{
	int			i;

	if (argc%2 != 1) {
		WRITE_MSG(2, "Bad parameter count\n");
		return EXIT_FAILURE;
	}

	for (i=1; i<argc; i+=2) {
		int		fd      = open(argv[i], O_WRONLY);
		char const *err_pos = 0;
		if (fd==-1) err_pos = "open";
		else if (write(fd, argv[i+1], strlen(argv[i+1]))==-1 ||
			 write(fd, "\n", 1)!=1)
			err_pos = "write";


		if (err_pos!=0) {
			size_t	l0 = strlen(err_pos);
			size_t	l1 = strlen(argv[i]);
			char	msg[l0+l1 + 3];

			strcpy(msg, err_pos);
			strcat(msg, "(");
			strcat(msg, argv[i]);
			strcat(msg, ")");

			perror(msg);
		}

		if (fd!=-1)
			close(fd);
	}
}
