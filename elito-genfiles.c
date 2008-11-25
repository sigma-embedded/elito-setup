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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define _GNU_SOURCE

#include <glob.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/mman.h>

struct token {
	char const	*ptr;
	size_t		len;
};

static struct err_info {
	char const	*file_name;
	unsigned int	line_num;
}	g_err;

#define TK_SKIP_NL	(1<<0)
#define P_ERR(X)	"%s:%u " X, g_err.file_name, g_err.line_num

static void
xperror(char const *msg)
{
	int		e = errno;

	if (g_err.line_num==0)
		fprintf(stderr, "%s: %s: %s\n",
			g_err.file_name, msg, strerror(e));
	else
		fprintf(stderr, "%s:%u %s: %s\n",
			g_err.file_name, g_err.line_num, msg, strerror(e));
}

static bool
token_get(char const *buf, char const *end_buf, struct token *res, unsigned long flags)
{
	while (buf < end_buf &&
	       (*buf==' ' || *buf=='\t' ||
		((flags & TK_SKIP_NL)!=0 && (*buf=='\n' || *buf=='\r')))) {
		if (*buf=='\n')
			++g_err.line_num;
		++buf;
	}

	if (buf==end_buf)
		return false;

	res->ptr = buf;

	while (buf < end_buf &&
	       !(*buf==' ' || *buf=='\t' || *buf=='\n' || *buf=='\r'))
		++buf;

	res->len = buf - res->ptr;
	return true;
}

static bool
token_cmp(struct token const *tk, char const *str, size_t str_len)
{
	if (str_len==(size_t)(-1))
		str_len = strlen(str);

	return str_len==tk->len && strncmp(tk->ptr, str, str_len)==0;
}

static char const *
token_get_multiple(char const *buf, char const *end_buf, struct token *res, size_t cnt)
{
	size_t			i;

	for (i=0; i<cnt; ++i) {
		if (!token_get(buf, end_buf, res+i, 0)) {
			fprintf(stderr, P_ERR("EOF while parsing line\n"));
			return NULL;
		}

		if (res[i].len==0) {
			fprintf(stderr, P_ERR("empty token seen at position %zu\n"),
				i);
			return NULL;
		}

		buf = res[i].ptr + res[i].len;
	}

	return buf;
}

static unsigned long
get_uid(char const *uid)
{
	struct passwd const	*pw = getpwnam(uid);
	if (pw)
		return pw->pw_uid;
	else
		return (unsigned long)-1;
}

static unsigned long
get_gid(char const *gid)
{
	struct group const	*gr = getgrnam(gid);
	if (gr)
		return gr->gr_gid;
	else
		return (unsigned long)-1;
}

static unsigned long
token_to_uint(struct token const *tk, int base, unsigned long (*xform)(char const *))
{
	char const		*tmp = strndupa(tk->ptr, tk->len);
	char			*err;
	unsigned long		res;

	res = strtoul(tmp, &err, base);
	if (*err && xform)
		res = xform(tmp);
	
	return res;
}

static char const *
do_file(char const *buf, char const *end_buf)
{
	struct token		args[4];
	char const		*name;
	mode_t			mode;
	uid_t			uid;
	gid_t			gid;

	int			fd;

	buf = token_get_multiple(buf, end_buf, args, sizeof(args)/sizeof(args[0]));
	if (!buf)
		return NULL;

	name = strndupa(args[0].ptr, args[0].len);
	mode = token_to_uint(args+1, 8, NULL);
	uid  = token_to_uint(args+2, 10, get_uid);
	gid  = token_to_uint(args+3, 10, get_gid);

	fd = open(name, O_WRONLY|O_NOFOLLOW|O_CREAT|O_EXCL, mode);
	if (fd<0) {
		xperror("open(<file>)");
		return buf;
	}

	if (fchown(fd, uid, gid)<0) {
		xperror("fchown(<file>)");
		close(fd);
		return buf;
	}

	close(fd);
	return buf;
}

static char const *
do_dir(char const *buf, char const *end_buf)
{
	struct token		args[4];
	char const		*name;
	mode_t			mode;
	uid_t			uid;
	gid_t			gid;

	buf = token_get_multiple(buf, end_buf, args, sizeof(args)/sizeof(args[0]));
	if (!buf)
		return NULL;

	name = strndupa(args[0].ptr, args[0].len);
	mode = token_to_uint(args+1, 8, NULL);
	uid  = token_to_uint(args+2, 10, get_uid);
	gid  = token_to_uint(args+3, 10, get_gid);

	if (mkdir(name, mode)<0) {
		xperror("mkdir(<dir>)");
		return buf;
	}

	if (chown(name, uid, gid)<0) {
		xperror("chown(<dir>)");
		return buf;
	}

	return buf;
}

static char const *
do_node(char const *buf, char const *end_buf)
{
	struct token		args[7];
	char const		*name;
	mode_t			mode;
	uid_t			uid;
	gid_t			gid;
	dev_t			dev;

	buf = token_get_multiple(buf, end_buf, args, sizeof(args)/sizeof(args[0]));
	if (!buf)
		return NULL;

	if (strchr("cb", args[4].ptr[0])==NULL) {
		fprintf(stderr, P_ERR("invalid device type\n"));
		return buf;
	}

	name = strndupa(args[0].ptr, args[0].len);
	mode = token_to_uint(args+1, 8, NULL);
	uid  = token_to_uint(args+2, 10, get_uid);
	gid  = token_to_uint(args+3, 10, get_gid);
	dev  = (args[4].ptr[0]=='c' ? S_IFCHR :
		args[4].ptr[0]=='b' ? S_IFBLK : S_IFBLK) |
		makedev(token_to_uint(args+5, 0, NULL),
			token_to_uint(args+6, 0, NULL));

	if (mknod(name, mode, dev)<0) {
		xperror("mknod(<nod>)");
		return buf;
	}

	if (chown(name, uid, gid)<0) {
		xperror("chown(<nod>)");
		return buf;
	}

	return buf;
}		

static char const *
do_slink(char const *buf, char const *end_buf)
{
	struct token		args[4];
	char const		*src;
	char const		*dst;
	uid_t			uid;
	gid_t			gid;

	buf = token_get_multiple(buf, end_buf, args, sizeof(args)/sizeof(args[0]));
	if (!buf)
		return NULL;

	src  = strndupa(args[0].ptr, args[0].len);
	dst  = strndupa(args[1].ptr, args[1].len);
	uid  = token_to_uint(args+2, 10, get_uid);
	gid  = token_to_uint(args+3, 10, get_gid);

	if (symlink(src, dst)<0) {
		xperror("symlink(<slink>");
		return buf;
	}

	if (lchown(dst, uid, gid)<0) {
		xperror("lchown(<slink>)");
		return buf;
	}

	return buf;
}
	
static char const *
do_pipe(char const *buf, char const *end_buf)
{
	struct token		args[4];
	char const		*name;
	mode_t			mode;
	uid_t			uid;
	gid_t			gid;

	buf = token_get_multiple(buf, end_buf, args, sizeof(args)/sizeof(args[0]));
	if (!buf)
		return NULL;

	name = strndupa(args[0].ptr, args[0].len);
	mode = token_to_uint(args+1, 8, NULL);
	uid  = token_to_uint(args+2, 10, get_uid);
	gid  = token_to_uint(args+3, 10, get_gid);

	if (mkfifo(name, mode)<0) {
		xperror("mkfifo(<pipe>)");
		return buf;
	}

	if (chown(name, uid, gid)<0) {
		xperror("chown(<pipe>)");
		return buf;
	}

	return buf;
}

static struct {
	char const	*cmd;
	char const *	(*handler)(char const *buf, char const *end_buf);
} const COMMANDS[] = {
	{ "file",  do_file },
	{ "dir",   do_dir },
	{ "nod",   do_node },
	{ "pipe",  do_pipe },
	{ "slink", do_slink },
};

static void
handle_data(char const *buf, size_t len)
{
	char const * const	end_buf = buf+len;
	struct token		cmd;

	while (token_get(buf, end_buf, &cmd, TK_SKIP_NL)) {
		buf = cmd.ptr + cmd.len;

		if (cmd.ptr[0]!='#') {
			char const *	(*handler)(char const *buf, char const *end_ptr);
			size_t		i;

			handler = NULL;
			for (i=0; i<sizeof(COMMANDS)/sizeof(COMMANDS[0]) && !handler; ++i)
				if (token_cmp(&cmd, COMMANDS[i].cmd, -1))
					handler = COMMANDS[i].handler;

			if (!handler)
				fprintf(stderr, P_ERR("unknown command '%.*s'\n"),
					(int)cmd.len, cmd.ptr);
			else {
				char const *	new_buf;
				new_buf = handler(buf, end_buf);

				if (new_buf)
					buf = new_buf;
			}
		}

		while (buf < end_buf && (*buf!='\r' && *buf!='\n'))
			++buf;
	}
}

static void
handle_file(char const *name)
{
	struct stat	st;
	int		fd;
	char		*buf;

	g_err.file_name = name;
	g_err.line_num  = 0;

	if (stat(name, &st)==-1) {
		xperror("stat()");
		return;
	}

	if (!S_ISREG(st.st_mode))
		return;
	
	fd = open(name, O_RDONLY|O_NOCTTY);
	if (fd<0) {
		xperror("open()");
		return;
	}

	buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!buf) {
		xperror("mmap()");
		close(fd);
		return;
	}

	g_err.line_num = 1;
	handle_data(buf, st.st_size);

	munmap(buf, st.st_size);
	close(fd);
}

int main(int argc, char *argv[])
{
	int	idx;

	umask(0);
	for (idx=1; idx<argc; ++idx) {
		size_t		i;
		glob_t		globbuf;

		if (glob(argv[idx], GLOB_ERR|GLOB_NOCHECK, NULL, &globbuf)<0) {
			perror("glob()");
			continue;
		}

		for (i=0; i<globbuf.gl_pathc; ++i)
			handle_file(globbuf.gl_pathv[i]);

		globfree(&globbuf);
	}
}
