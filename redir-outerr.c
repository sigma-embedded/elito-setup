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
