#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
  int i=0, n=0, m=0;
  char *args[MAXARG];
  char buf[1024];
  char *p, *q;

  if (argc < 2) {
    printf("xarg: too few arguments");
    exit(0);
  }
  if (argc >= MAXARG) {
    printf("xarg: too many arguments");
    exit(0);
  }

  // Copying arguments
  for (i=0;i<argc-1;i++) {
    args[i] = argv[i+1];
  }
  args[i+1]=0;

  //Reading input
  while ((n=read(0, buf+m, sizeof(buf)-m-1))>0) {
      m+=n;
      buf[m] = '\0';
      p = buf;
      while ((q=strchr(p, '\n'))) {
        *q = '\0';
        args[i] = p;
        if(fork()) {
          wait(0);
        }
        else {
          exec(args[0], args);
          exit(0);
        }
        p = q + 1;
      }
      if (m>0) {
        m -= p - buf;
        memmove(buf, p, m);
      }
  }

  exit(0);
}
