#define _GNU_SOURCE
#define _ATFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <dirent.h>

#include "util.h"

#ifndef NFS_SUPER_MAGIC
#  define NFS_SUPER_MAGIC	0x6969
#endif

#ifndef TMPFS_MAGIC
#  define TMPFS_MAGIC	0x01021994
#endif

#ifndef SYSTEMD_TEMPLATE_DIR
#  define SYSTEMD_TEMPLATE_DIR	"/usr/share/elito-systemd"
#endif

#ifndef SYSTEMD_INIT_PROG
#  define SYSTEMD_INIT_PROG	"/lib/systemd/systemd"
#endif

static struct mountpoint_desc const	G_MOUNTPOINTS[] = {
	{ "devtmpfs", "/dev", "tmpfs", "size=512k,mode=755", MS_NOSUID },
	{ "proc", "/proc", "proc", NULL, MS_NOSUID|MS_NODEV|MS_NOEXEC },
};

static struct mountpoint_desc const	G_MOUNTPOINTS_SYSTEMD[] = {
	{ "tmpfs", "/run", "tmpfs", "mode=755", MS_NOSUID|MS_NODEV },
};

inline static bool is_nfs_boot(void)
{
	static int	res = -1;
	struct statfs	st;

	if (res == -1) {
		if (statfs("/", &st) < 0)
			abort();

		res = (st.f_type == NFS_SUPER_MAGIC) ? 1 : 0;
	}

	return res > 0;
}

inline static bool is_nfs_ro_boot(void)
{
	static int	res = -1;

	if (res == -1) {
		if (!is_nfs_boot())
			res = 0;
		else
			res = (access("/", W_OK) < 0 &&
			       (errno == EROFS || errno == EACCES));
	}

	return res;
}

static void xclose(int fd)
{
	if (fd != -1)
		close(fd);
}

static bool copy_reg_fd(int src, int dst, off_t len)
{
	fallocate(dst, 0, 0, len);	/* ignore errors */

#if 0
	int	rc = ftruncate(dst, len);
	void	*src_mem = mmap(NULL, len, PROT_READ, MAP_SHARED, src, 0);
	void	*dst_mem = mmap(NULL, len, PROT_WRITE, MAP_SHARED, dst, 0);
	bool	res = false;

	if (rc < 0 || !src_mem || !dst_mem)
		goto out;

	memcpy(dst_mem, src_mem, len);
	res = true;

out:
	if (dst_mem)
		munmap(dst_mem, len);
	if (src_mem)
		munmap(src_mem, len);

	return res;
#else
	return sendfile(dst, src, NULL, len) == len;
#endif
}

static bool copy_dir_fd(int src, int dst)
{
	DIR		*src_dir = fdopendir(dup(src));
	struct dirent	*ent;
	bool		res = false;

	if (!src_dir)
		goto out;

	while ((ent = readdir(src_dir))) {
		struct stat	st;
		int		rc;
		mode_t		file_mode;
		bool		need_mode = false;
		bool		need_owner = false;

		if (ent->d_name[0] == '.' &&
		    ((ent->d_name[1] == '\0') ||
		     (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
			continue;


		rc = fstatat(src, ent->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (rc < 0)
			goto out;

		file_mode = st.st_mode & (S_ISUID | S_ISGID | S_ISVTX |
					  0777);

		unlinkat(dst, ent->d_name, 0); /* ignore errors */

		if (S_ISDIR(st.st_mode)) {
			rc = mkdirat(dst, ent->d_name, 0700);
			if (rc == 0) {
				need_mode = true;
				need_owner = true;
			} else if (errno == EEXIST)
				rc = 0;
		}

		if (rc < 0)
			goto out;

		if (S_ISDIR(st.st_mode)) {
			int	new_src = openat(src, ent->d_name,
						 O_DIRECTORY|O_RDONLY|O_NOFOLLOW);
			int	new_dst = openat(dst, ent->d_name,
						 O_DIRECTORY|O_RDONLY|O_NOFOLLOW);

			if (new_src == -1 ||
			    new_dst == -1 ||
			    !copy_dir_fd(new_src, new_dst))
				rc = -1;

			xclose(new_dst);
			xclose(new_src);
		} else if (S_ISLNK(st.st_mode)) {
			ssize_t	l;
			char	buf[PATH_MAX + 1];

			l = readlinkat(src, ent->d_name, buf, sizeof buf);
			if (l < 0 || l == sizeof buf) {
				buf[0] = '\0';
				rc = -1;
			} else
				buf[l] = '\0';

			rc = symlinkat(buf, dst, ent->d_name);

			need_owner = true;
		} else if (S_ISREG(st.st_mode)) {
			int	new_src = openat(src, ent->d_name,
						 O_RDONLY|O_NOFOLLOW);
			int	new_dst = openat(dst, ent->d_name,
						 O_WRONLY|O_CREAT|O_TRUNC|
						 O_NOFOLLOW, 0600);

			if (new_src == -1 ||
			    new_dst == -1 ||
			    !copy_reg_fd(new_src, new_dst, st.st_size))
				rc = -1;

			xclose(new_dst);
			xclose(new_src);

			need_mode = true;
			need_owner = true;
		} else
			continue;	/* noop; unsupported filetype */

		if (rc >= 0 && need_owner)
			rc = fchownat(dst, ent->d_name, st.st_uid, st.st_gid,
				      AT_SYMLINK_NOFOLLOW);

		if (rc >= 0 && need_mode) {
			rc = fchmodat(dst, ent->d_name, file_mode,
				      AT_SYMLINK_NOFOLLOW);

			/* HACK: AT_SYMLINK_NOFOLLOW is not supported for
			 * fchmodat */
			if (rc < 0 && errno == ENOTSUP)
				rc = fchmodat(dst, ent->d_name, file_mode, 0);
		}

		if (rc < 0)
			goto out;
	}

	res = true;

out:
	closedir(src_dir);
	return res;
}

static bool copy_dir(char const *src, char const *dst, bool strict)
{
	int	src_fd = open(src, O_DIRECTORY|O_RDONLY|O_NOFOLLOW);
	int	dst_fd = open(dst, O_DIRECTORY|O_RDONLY|O_NOFOLLOW);
	int	res;

	if (src_fd != -1 && dst_fd != -1)
		res = copy_dir_fd(src_fd, dst_fd);
	else
		res = !strict;

	xclose(dst_fd);
	xclose(src_fd);

	return res;
}

static bool write_all(int fd, void const *buf, size_t len)
{
	while (len > 0) {
		ssize_t		l = write(fd, buf, len);

		if (l > 0) {
			buf += l;
			len -= l;
		} else if (l < 0 && errno == EINTR) {
			continue;
		} else {
			return false;
		}
	}

	return true;
}

static bool write_str(int fd, char const *str)
{
	return write_all(fd, str, strlen(str));
}

static bool setup_systemd(void)
{
	if (!enable_systemd())
		return true;

	if (mount_set(NULL, G_MOUNTPOINTS_SYSTEMD,
		      ARRAY_SIZE(G_MOUNTPOINTS_SYSTEMD)) < 0)
		return false;

	if (is_nfs_ro_boot())
		xclose(open("/run/nfs-ro-boot", O_CREAT|O_NOFOLLOW|O_WRONLY,
			    0644));

	mkdir("/run/systemd", 0755);
	mkdir("/run/systemd/system", 0755);
	mkdir("/run/systemd/journal", 0755);

	if (is_nfs_ro_boot() &&
	    !copy_dir(SYSTEMD_TEMPLATE_DIR "/nfs", "/run/systemd/system", false))
		return false;

	return true;
}

int main(int argc, char *argv[])
{
	int		mres[ARRAY_SIZE(G_MOUNTPOINTS)];

	setresuid(0,0,0);
	setresgid(0,0,0);
	umask(0);

	if (mount_set(mres, G_MOUNTPOINTS, ARRAY_SIZE(G_MOUNTPOINTS)) < 0)
		return EXIT_FAILURE;

	/* HACK: assume that element #0 is the devtmpfs entry */
	switch (mres[0]) {
	case 0:
		if (mknod("/dev/console", 0600 | S_IFCHR, makedev(5,1))==-1 ||
		    mknod("/dev/null",    0666 | S_IFCHR, makedev(1,3))==-1)
			return EXIT_FAILURE;
		break;

	case 1:
		/* noop; already handled by something else */
		break;

	default:
		return EXIT_FAILURE;
	}

	close(0);
	close(1);
	close(2);

	if (open("/dev/console", O_RDONLY)!=0 ||
	    open("/dev/console", O_WRONLY)!=1 ||
	    dup2(1,2)==-1)
		return EXIT_FAILURE;

	if (!setup_systemd())
		return EXIT_FAILURE;

	umask(022);

	execv("/sbin/init.wrapped", argv);
	if (enable_systemd())
		execv(SYSTEMD_INIT_PROG, argv);

	return EXIT_FAILURE;
}
