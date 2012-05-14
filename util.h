/*	--*- c -*--
 * Copyright (C) 2012 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
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

#ifndef H_ELITO_SETUP_TOOLS_UTIL_H
#define H_ELITO_SETUP_TOOLS_UTIL_H

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>

#define ARRAY_SIZE(_a)	(sizeof(_a) / sizeof((_a)[0]))

inline static bool enable_systemd(void)
{
#ifdef ENABLE_SYSTEMD
	return true;
#else
	return false;
#endif
}

struct mountpoint_desc {
	char const	*source;
	char const	*target;
	char const 	*type;
	char const	*options;
	unsigned long	flags;
};

static bool is_mount_point(char const *path)
{
	struct stat	st;
	struct stat	st_parent;
	char		tmp[strlen(path) + sizeof("/..")];
	char		*p;
	size_t		l;

	if (lstat(path, &st) < 0)
		return false;

	l = strlen(path);
	memcpy(tmp, path, l+1);
	while (l > 0 && tmp[l-1] == '/')
		--l;
	tmp[l] = '\0';

	p = strrchr(tmp, '/');
	if (p == NULL)
		strcpy(tmp, "..");
	else if (p == tmp)
		/* handle mountpoints directly below / */
		p[1] = '\0';
	else
		p[0] = '\0';

	if (lstat(tmp, &st_parent) < 0)
		return false;

	return st.st_dev != st_parent.st_dev;
}

static int mount_one(struct mountpoint_desc const *desc)
{
	if (is_mount_point(desc->target))
		return 1;

	return mount(desc->source, desc->target, desc->type,
		     desc->flags, desc->options) == -1 ? -1 : 0;
}

static int mount_set(int *mres,
		     struct mountpoint_desc const descs[],
		     size_t cnt)
{
	size_t		i;
	for (i = 0; i < cnt; ++i) {
		int	rc;
		rc = mount_one(&descs[i]);

		if (rc < 0)
			return rc;

		if (mres)
			mres[i] = rc;
	}

	return 0;
}

#endif	/* H_ELITO_SETUP_TOOLS_UTIL_H */
