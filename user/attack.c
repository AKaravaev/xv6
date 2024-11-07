#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *page;
  char secret[8];
  secret[0] = '\0';
  while (strlen(secret)!=7)
  {
    page = sbrk(PGSIZE);
    strcpy((char *)secret, page+32);
  }
  printf ("Found: %s\n", secret);
  write(2, (char *)secret, 8);

  exit(1);
}
