#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char* path, char* filename) {
  char buf[512], *p;
  int fd;
  struct stat st;
  struct dirent de;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type != T_DIR) {
    fprintf(2, "find: %s is not a directory\n", path);
    close(fd);
    return;
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
    printf("find: path too long\n");
  }

  strcpy(buf, path);
  p = buf+strlen(buf);
  *p++ = '/';
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if(stat(buf, &st) < 0){
      printf("find: cannot stat %s\n", buf);
      continue;
    }
    switch(st.type){
      case T_DIR:
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
          break;
        find(buf, filename);
        break;
      case T_FILE:
        if(strcmp(filename, de.name) == 0)
          printf("%s\n", buf);
        break;
    }
  }
  close(fd);
}

int main(int argc, char* argv[])
{
  if (argc<3){
    fprintf(2, "Usage: find <dir> <filename>\n");
  }
  else {
    find(argv[1], argv[2]);
  }
  exit(0);
}
