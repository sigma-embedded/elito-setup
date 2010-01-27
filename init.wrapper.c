#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{
	struct stat	st;

	if (stat("/dev/null", &st) < 0 &&
	    (mount("none", "/dev", "tmpfs", 0, "size=512k")==-1 ||
	     mknod("/dev/console", 0600 | S_IFCHR, makedev(5,1))==-1 ||
	     mknod("/dev/null",    0666 | S_IFCHR, makedev(1,3))==-1))
		return EXIT_FAILURE;

	close(0);
	close(1);
	close(2);

	if (open("/dev/console", O_RDONLY)==-1 ||
	    open("/dev/console", O_WRONLY)==-1 ||
	    dup2(1,2)==-1 ||
	    execl("/sbin/init.wrapped", "/sbin/init", NULL)==-1)
		return EXIT_FAILURE;
}
