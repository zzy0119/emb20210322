#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(void)
{
	int shmid;
	pid_t pid;
	char *ptr;

	shmid = shmget(IPC_PRIVATE, 1024, IPC_CREAT | IPC_EXCL | 0600);
	if (-1 == shmid) {
		perror("shmget()");	
		exit(1);
	}

	pid = fork();
	if (-1 == pid) {
		perror("fork()");
		goto ERROR;
	}
	if (0 == pid) {
		ptr = (char *)shmat(shmid, NULL, 0);
		if (NULL == ptr) {
			perror("shmat()");	
			goto ERROR;
		}
		memcpy(ptr, "class is over", 13);
		shmdt(ptr);
		exit(0);
	}
	wait(NULL);

	ptr = shmat(shmid, NULL, 0);
	puts(ptr);
	shmdt(ptr);

	shmctl(shmid, IPC_RMID, NULL);

	exit(0);
ERROR:
	shmctl(shmid, IPC_RMID, NULL);
	exit(1);
}

