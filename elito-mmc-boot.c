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

#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <sys/socket.h>

/* dietlibc hack */
typedef unsigned short __kernel_sa_family_t;

#include <linux/netlink.h>

#include "util.h"

#define BUF_SIZE	4096
#define MAX_PATH	256

#define PIVOT_MNTPOINT	"/mnt"

extern int pivot_root(const char *new_root, const char *put_old);

static struct mountpoint_desc const	G_MOUNTPOINTS[] = {
	{ "devtmpfs", "/dev", "devtmpfs", "size=512k,mode=755", MS_NOSUID },
	{ "proc", "/proc", "proc", NULL, MS_NOSUID|MS_NODEV|MS_NOEXEC },
	{ "sysfs", "/sys", "sysfs", NULL, MS_NOSUID|MS_NODEV|MS_NOEXEC },
#ifdef ENABLE_SYSTEMD
	{ "tmpfs", "/run", "tmpfs", "mode=755", MS_NOSUID|MS_NODEV },
#endif
};

static struct mountpoint_desc const	G_MOUNTPOINTS_SYSTEMD[] = {
};

struct bootoptions {
	char const	*device;
	char const	*partition;
	char const	*fstype;

	struct {
		char	cmdline[BUF_SIZE];
		char	devname[MAX_PATH];
	}		buffers;
};

static void write_msg(int fd, char const *msg)
{
	write(fd, msg, strlen(msg));
}

static char *find_cmdline_option(char *buf, char const *key)
{
	char		*ptr;

	ptr = strstr(buf, key);
	if (ptr != NULL)
		ptr += strlen(key);

	return ptr;
}

static int get_boot_device(struct bootoptions *opts)
{
	char		*buffer = opts->buffers.cmdline;
	int		fd;
	char		*root;
	char		*rootfstype;
	char		*ptr;
	ssize_t		l;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0) {
		perror("open(/proc/cmdline)");
		return -1;
	}

	l = read(fd, buffer+1, sizeof opts->buffers.cmdline - 2);
	close(fd);

	if (l < 0) {
		perror("read(/proc/cmdline)");
		return -1;
	}
	buffer[0] = ' ';
	buffer[l] = '\0';

	root = find_cmdline_option(buffer, " root=");
	if (root == NULL) {
		write(2, "missing 'root=' option\n", 23);
		return -1;
	}

	rootfstype = find_cmdline_option(buffer, " rootfstype=");

	/* No find_cmdline_option() behind this line because Buffer will be
	 * modified */

	ptr = strchr(root, ' ');
	if (ptr != NULL)
		*ptr = '\0';

	ptr = rootfstype ? strchr(rootfstype, ' ') : NULL;
	if (ptr != NULL)
		*ptr = '\0';

	ptr = strchr(root, '!');
	if (ptr != 0) {
		*ptr = '\0';
		++ptr;
	}

	opts->device   = root;
	opts->partition = ptr;

	if (rootfstype && rootfstype[0] != '\0')
		opts->fstype = rootfstype;

	return 0;
}

static char const *check_dir(char const *name, struct bootoptions *opts)
{
	char		*buffer = opts->buffers.devname;
	char		lnk_path[MAX_PATH];
	char		part_fname[strlen(name) + sizeof("/partition")];
	char		part[32];
	int		fd;

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return NULL;

	if (readlink(name, lnk_path, sizeof lnk_path) < 0)
		return NULL;

	if (strstr(lnk_path, opts->device) == NULL)
		return NULL;

	strcpy(part_fname, name);
	strcat(part_fname, "/partition");

	fd = open(part_fname, O_RDONLY);
	if (fd < 0)
		part[0] = '\0';
	else {
		ssize_t		l = read(fd, part, sizeof part - 1);
		close(fd);

		if (l < 0)
			return NULL;

		while (l > 0 && part[l-1] == '\n')
			--l;

		part[l] = '\0';
	}

	if (opts->partition == NULL && part[0] != '\0')
		return NULL;

	if (opts->partition != NULL && strcmp(opts->partition, part) != 0)
		return NULL;

	strcpy(buffer, "/dev/");
	strcat(buffer, name);

	return buffer;
}

int main(int argc, char *argv[]) {
	struct bootoptions	bootopts = {
		.fstype = "ext4"
	};

	struct dirent	**namelist;
	ssize_t		num_names;
	char const	*boot_dev = NULL;
	int		sock;
	size_t		i;
	int		kmsg_fd;

	struct sockaddr_nl		snl = {
		.nl_family	=  AF_NETLINK,
		.nl_pid		=  getpid(),
		.nl_groups	=  0xffffffffu,
	};

	setresuid(0,0,0);
	setresgid(0,0,0);
	umask(0);

	if (mount_set(NULL, G_MOUNTPOINTS, ARRAY_SIZE(G_MOUNTPOINTS)) < 0)
		return EXIT_FAILURE;

	close(0);
	close(1);
	close(2);

	open("/dev/console", O_RDONLY);
	open("/dev/console", O_WRONLY);
	open("/dev/console", O_WRONLY);

	if (get_boot_device(&bootopts) < 0)
		return EXIT_FAILURE;

	if (chdir("/sys/class/block") < 0) {
		perror("chdir(/sys/class/block)");
		return EXIT_FAILURE;
	}

	sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	bind(sock, (void *)&snl, sizeof snl);

	kmsg_fd = open("/dev/kmsg", O_WRONLY);
	write_msg(kmsg_fd, "<4>Waiting for bootdevice\n");

	num_names = scandir(".", &namelist, NULL, NULL);
	if (num_names >= 0) {
		while (num_names > 0 && boot_dev == NULL) {
			boot_dev = check_dir(namelist[num_names-1]->d_name,
					     &bootopts);
			free(namelist[num_names-1]);
			--num_names;
		}
		free(namelist);
	}

	while (boot_dev == NULL) {
		char		buf[4096 + 1];
		char		*ptr;
		char const	*op = NULL;
		char const	*devname = NULL;

		ssize_t		l;

		l = read(sock, &buf, sizeof buf - 1);
		if (l < 0)
			continue;

		buf[l] = '\0';
		if (strncmp(buf, "add@", 4) != 0)
			continue;

		ptr = buf;
		while (*ptr && ptr < buf+l) {
			size_t		l1;

			l1 = strlen(ptr);
			if (ptr == buf)
				op = ptr;
			else if (strncmp(ptr, "DEVNAME=", 8) == 0)
				devname = ptr + 8;

			ptr += l1 + 1;
		}

		if (devname == NULL)
			continue;

		boot_dev = check_dir(devname, &bootopts);
	}

	close(sock);

	if (access(boot_dev, F_OK) < 0) {
		perror("access(<bootdev>)");
		return EXIT_FAILURE;
	}

	if (mount(boot_dev, PIVOT_MNTPOINT, bootopts.fstype,
		  MS_MGC_VAL | MS_RDONLY, NULL) < 0) {
		perror("mount()");
		return EXIT_FAILURE;
	}

	if (chdir(PIVOT_MNTPOINT) < 0) {
		perror("chdir(<PIVOT_MNTPOINT>)");
		return EXIT_FAILURE;
	}

	for (i = ARRAY_SIZE(G_MOUNTPOINTS); i > 0; --i) {
		char const	*mp = G_MOUNTPOINTS[i-1].target;

		if (mount(mp, mp+1, NULL, MS_MOVE, NULL) < 0) {
			perror("mount(<MS_MOVE>)");
			return EXIT_FAILURE;
		}

		/* \todo: add diagnostics */
		rmdir(mp);
	}

	/* \todo: add diagnostics */
	unlink("/dev/console");
	unlink("/init");
	rmdir("/dev");

	if (mount(".", "/", NULL, MS_MOVE, NULL) < 0) {
		perror("mount(\".\", \"/\", MS_MOVE)");
		return EXIT_FAILURE;
	}

	if (chroot(".") < 0 || chdir("/") < 0) {
		perror("chroot()");
		return EXIT_FAILURE;
	}

	if (enable_systemd()) {
		/* \todo: add diagnostics */
		close(open("/run/elito-mmc-boot", O_CREAT|O_WRONLY, 0644));
	}

	write_msg(kmsg_fd, "<5>jumping into real init\n");
	close(kmsg_fd);

	argv[0] = "/sbin/init";
	execve("/sbin/init", argv, environ);
	perror("execv()");
	return EXIT_FAILURE;
}
