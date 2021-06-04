#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>

#define BUFSIZE	1024
#define TTY9	"/dev/tty11"
#define TTY10	"/dev/tty12"

enum {STATE_R, STATE_W, STATE_E, STATE_T};

// 状态机
typedef struct {
	int rfd;
	int wfd;
	char buf[BUFSIZE];
	int readCnt; // 读到的字节个数	
	int state; // 状态机的状态
	char *errmsg;
	int pos; // 写偏移量
}fsm_t;

// 状态机推动
static int fsmDrive(fsm_t *fsm)
{
	int ret;

	switch (fsm->state) {
		case STATE_R:
			fsm->readCnt = read(fsm->rfd, fsm->buf, BUFSIZE);
			if (fsm->readCnt == -1) {
				if (errno != EAGAIN) {
					fsm->errmsg = "read";
					fsm->state = STATE_E;
				}
			} else if (fsm->readCnt == 0) {
				fsm->state = STATE_T;
			} else {
				fsm->pos = 0;
				fsm->state = STATE_W;
			}
			break;
		case STATE_W:
			ret = write(fsm->wfd, fsm->buf+fsm->pos, fsm->readCnt);
			if (ret == -1) {
				fsm->state = STATE_E;
				fsm->errmsg = "write()";
			} else if (ret < fsm->readCnt) {
				fsm->readCnt -= ret;
				fsm->pos += ret;
			} else {
				fsm->state = STATE_R;
			}
			break;
		case STATE_E:
			perror(fsm->errmsg);
			fsm->state = STATE_T;
			break;
		case STATE_T:
			// exit(0);
			break;
		default:
			break;	
	}

	return 0;
}

static void fsmInit(fsm_t *fsm, int rfd, int wfd)
{
	fsm->state = STATE_R;
	memset(fsm->buf, '\0', BUFSIZE);
	fsm->rfd = rfd;
	fsm->wfd = wfd;
	fsm->errmsg = NULL;
	fsm->readCnt = 0;
	fsm->pos = 0;

}

static int fsmCreate(int fd1, int fd2)
{
	fsm_t fsm12, fsm21;
	int flag;
	int fd1_save, fd2_save;
	fd_set rset, wset;

	// 确保fd1 fd2非阻塞io
	fd1_save = fcntl(fd1, F_GETFL);
	fcntl(fd1, F_SETFL, fd1_save | O_NONBLOCK);
	fd2_save = fcntl(fd2, F_GETFL);
	fcntl(fd2, F_SETFL, fd2_save | O_NONBLOCK);

	fsmInit(&fsm12, fd1, fd2);
	fsmInit(&fsm21, fd2, fd1);

	while (fsm12.state != STATE_T || fsm21.state != STATE_T) {
		if (fsm12.state == STATE_E) {
			fsmDrive(&fsm12);
			continue;
		}
		if (fsm21.state == STATE_E) {
			fsmDrive(&fsm21);
			continue;
		}
		// 初始化文件描述符集
		FD_ZERO(&rset);
		FD_ZERO(&wset);
		if (fsm12.state == STATE_R)
			FD_SET(fsm12.rfd, &rset);
		else if (fsm12.state == STATE_W)
			FD_SET(fsm12.wfd, &wset);
		if (fsm21.state == STATE_R)
			FD_SET(fsm21.rfd, &rset);
		else if (fsm21.state == STATE_W)
			FD_SET(fsm21.wfd, &wset);

		if (select((fd1>fd2?fd1:fd2) + 1, &rset, &wset, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			perror("select()");
			goto ERROR;
		}

		if (FD_ISSET(fsm12.rfd, &rset) || FD_ISSET(fsm12.wfd, &wset))
			fsmDrive(&fsm12);	
		if (FD_ISSET(fsm21.rfd, &rset) || FD_ISSET(fsm21.wfd, &wset))
			fsmDrive(&fsm21);	
	}

	return 0;

ERROR:
	return -1;
}

int main(void)
{
	int fd1, fd2;

	fd1 = open(TTY9, O_RDWR);
	if (-1 == fd1) {
		perror("open()");
		exit(1);
	}
	write(fd1, "*****[tty9]*****\n", 17);

	fd2 = open(TTY10, O_RDWR | O_NONBLOCK);
	if (-1 == fd2) {
		perror("open()");
		close(fd1);
		exit(1);
	}
	write(fd2, "*****[tty10]*****\n", 18);

	fsmCreate(fd1, fd2);

	exit(0);
}

