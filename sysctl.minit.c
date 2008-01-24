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
