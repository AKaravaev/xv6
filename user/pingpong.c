#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
	int p[2];
	char byte = '\x01';

	pipe(p);
	if (fork() > 0){
		//Parent
		write(p[1],&byte,1);
		wait(0);
		read(p[0],&byte,1);
		printf("%d: received pong\n", getpid());
	}
	else {
		//Child
		read(p[0],&byte,1);
		printf("%d: received ping\n", getpid());
		write(p[1],&byte,1);
	}
	exit(0);
}
