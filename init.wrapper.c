#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/vfs.h>

#ifndef TMPFS_MAGIC
#  define TMPFS_MAGIC	0x01021994
#endif

int main()
{
	struct statfs	st;

	if ((statfs("/dev", &st) < 0 || st.f_type != TMPFS_MAGIC) &&
	    (mount("none", "/dev", "tmpfs", 0, "size=512k")==-1 ||
	     mknod("/dev/console", 0600 | S_IFCHR, makedev(5,1))==-1 ||
	     mknod("/dev/null",    0666 | S_IFCHR, makedev(1,3))==-1))
		return EXIT_FAILURE;

	close(0);
	close(1);
	close(2);

	if (open("/dev/console", O_RDONLY)!=0 ||
	    open("/dev/console", O_WRONLY)!=1 ||
	    dup2(1,2)==-1)
		return EXIT_FAILURE;

	execl("/sbin/init.wrapped", "/sbin/init", NULL);
	return EXIT_FAILURE;
}
