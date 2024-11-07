#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
	int p[2];
	int lfd, rfd;
	int i,n = 0;
	lfd = rfd = 0;

	pipe(p);
	if (fork()){
		//Parent
		close(p[0]);
		rfd=p[1];
		for(i = 2;i <= 280;i++){
			write(rfd,&i,sizeof(int));
		}
	}
	else {
		//Child
		close(p[1]);
		lfd = p[0];
		while(read(lfd,&i,sizeof(int))){
			if(!n) {
				n = i;
				printf("prime %d\n",n);
			}
			else if(i%n) {
				if(!rfd){
					pipe(p);
					if (fork()){
						close(p[0]);
						rfd = p[1];
					}
					else {
						close(p[1]);
						close(lfd);
						close(p[1]);
						lfd = p[0];
						rfd = 0;
						n = 0;
					}
				}
				if(rfd) write(rfd,&i,sizeof(int));
			}
		}
		close(lfd);
	}
	if(rfd) close(rfd);
	wait(0);
	exit(0);
}
