#define _GNU_SOURCE
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/un.h>
#include <x86intrin.h>
#include <inttypes.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#define MAX_SIZE 8192
#define PF_MAX_SIZE 100 * 4096
#define LOOP 1000
#define STEP 256
#define PF_STEP 4096
#define CENT ((MAX_SIZE / STEP) / 100)
#define PF_CENT ((PF_MAX_SIZE / PF_STEP) / 100)

#define ADDR_HINT 0x300000000000

#define CPU1 14

FILE *fp;

//---------------------------------------------------------------------
#ifdef USE_VMALLOC
extern void *vmalloc(unsigned long size);
extern void vfree(const void *addr);
#endif
//---------------------------------------------------------------------
#ifdef BYPASS
extern ssize_t bp_write(int fd, const void *buf, size_t count);
extern ssize_t bp_read(int fd, void *buf, size_t count);
extern void *bp_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int bp_munmap(void *addr, size_t length);
extern int bp_select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict exceptfds, struct timeval *restrict timeout);
extern int bp_poll(struct pollfd *fds, nfds_t nfds, int timeout);
extern int bp_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
extern pid_t bp_getppid(void);
extern ssize_t bp_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
extern ssize_t bp_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);
#endif
//---------------------------------------------------------------------
#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif
//---------------------------------------------------------------------

void calc_diff(struct timespec *diff, struct timespec *bigger, struct timespec *smaller)
{
	if (smaller->tv_nsec > bigger->tv_nsec)
	{
		diff->tv_nsec = 1000000000 + bigger->tv_nsec - smaller->tv_nsec;
		diff->tv_sec = bigger->tv_sec - 1 - smaller->tv_sec;
	}
	else
	{
		diff->tv_nsec = bigger->tv_nsec - smaller->tv_nsec;
		diff->tv_sec = bigger->tv_sec - smaller->tv_sec;
	}
}

void calc_sum(struct timespec *sum, struct timespec *bigger, struct timespec *smaller)
{
	sum->tv_sec = 0;
	sum->tv_nsec = 0;
	if (smaller->tv_nsec >= (1000000000 - bigger->tv_nsec))
	{
		bigger->tv_sec = bigger->tv_sec + 1;
		sum->tv_nsec = smaller->tv_nsec - (1000000000 - bigger->tv_nsec);
		sum->tv_sec = bigger->tv_sec + smaller->tv_sec;
	}
	else
	{

		sum->tv_nsec = bigger->tv_nsec + smaller->tv_nsec;
		sum->tv_sec = bigger->tv_sec + smaller->tv_sec;
	}
}

void add_diff_to_sum(struct timespec *result, struct timespec *a, struct timespec *b)
{
	struct timespec diff;
	struct timespec tmp;
	calc_diff(&diff, a, b);
	tmp.tv_sec = result->tv_sec;
	tmp.tv_nsec = result->tv_nsec;
	calc_sum(result, &tmp, &diff);
}

void calc_average(struct timespec *average, struct timespec *sum, int size)
{
	average->tv_sec = 0;
	average->tv_nsec = 0;
	if (size == 0)
		return;

	average->tv_nsec = sum->tv_nsec / size + sum->tv_sec % size * 1000000000 / size;
	average->tv_sec = sum->tv_sec / size;
}

//---------------------------------------------------------------------

void getppid_bench(void)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	int l;
	int loop = 100000;

	for (l = 0; l < loop; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef BYPASS
		bp_getppid();
#else
		syscall(SYS_getppid);
#endif
		clock_gettime(CLOCK_MONOTONIC, &endTime);

		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%ld.%09ld\n", l, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}

	return;
}

void clock_bench(void)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	int l;
	int loop = 100000;

	for (l = 0; l < loop; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
		clock_gettime(CLOCK_MONOTONIC, &endTime);

		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%ld.%09ld\n", l, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}
	return;
}

void cpu_bench(void)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	int i, l;
	double start;
	double div;
	int loop = 1000;

	for (l = 0; l < loop; l++)
	{
		start = 9903290.789798798;
		div = 3232.32;

		clock_gettime(CLOCK_MONOTONIC, &startTime);
		for (i = 0; i < 500000; i++)
		{
			start = start / div;
		}
		clock_gettime(CLOCK_MONOTONIC, &endTime);

		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%ld.%09ld\n", l, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}
	return;
}

void write_bench(int file_size)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	char *buf;
	int fd, i, l;

#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * file_size);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * file_size);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * file_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	for (i = 0; i < file_size; i++)
	{
		buf[i] = i % 93 + 33;
	}
	fd = open("test_file.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		printf("invalid fd in write: %d\n", fd);
		exit(0);
	}

	for (l = 0; l < LOOP * 10; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef BYPASS
		bp_write(fd, buf, file_size);
#else
		syscall(SYS_write, fd, buf, file_size);
#endif
		clock_gettime(CLOCK_MONOTONIC, &endTime);
		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}

	close(fd);

#if defined(USE_VMALLOC)
	vfree(buf);
#elif defined(USE_MALLOC)
	free(buf);
#else
	syscall(SYS_munmap, buf, file_size);
#endif

	return;
}

void read_bench(int file_size)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	char *buf;
	int fd, l, i;

#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * file_size);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * file_size);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * file_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	for (i = 0; i < file_size; i++)
	{
		buf[i] = i % 93;
	}
	fd = open("test_file.txt", O_RDONLY);
	if (fd < 0)
	{
		perror("invalid fd in read\n");
		exit(0);
	}

	for (l = 0; l < LOOP * 10; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef BYPASS
		bp_read(fd, buf, file_size);
#else
		syscall(SYS_read, fd, buf, file_size);
#endif
		clock_gettime(CLOCK_MONOTONIC, &endTime);
		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}

	close(fd);

#if defined(USE_VMALLOC)
	vfree(buf);
#elif defined(USE_MALLOC)
	free(buf);
#else
	syscall(SYS_munmap, buf, file_size);
#endif

	return;
}

#define sock "./my_sock"
void send_bench(int msg_size)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};

	// need to set affinity of parent and child to different cpus
	int retval, forkId, status, l;
	int fds1[2], fds2[2];
	char w = 'b', r;
	char *buf;
	struct sockaddr_un server_addr;

	int recvbuff, retsock, sendbuff, newbuff;
	socklen_t optlen = sizeof(recvbuff);

	cpu_set_t cpuset;
	int prio;

	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;

	strncpy(server_addr.sun_path, sock, sizeof(server_addr.sun_path) - 1);

	// create a buffer (this needs to be mmap)
#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * msg_size);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * msg_size);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * msg_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	// write character 'a' in the buffer
	for (int i = 0; i < msg_size; i++)
	{
		buf[i] = 'a';
	}

	for (l = 0; l < LOOP; l++)
	{
		retval = pipe(fds1);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");
		retval = pipe(fds2);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");

		forkId = fork();

		if (forkId < 0)
		{
			printf("[error] fork failed.\n");
			return;
		}
		if (forkId == 0)
		{
			close(fds1[0]); // close the read end of pipe 1
			close(fds2[1]); // close the write end of pipe 2

			int fd_server = socket(AF_UNIX, SOCK_STREAM, 0); // create a socket for server
			if (fd_server < 0)
				printf("[error] failed to open server socket.\n");

			// bind the socket to a filename so communication can occur
			retval = bind(fd_server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to bind.\n");

			// Listen on the socket
			retval = listen(fd_server, 10);
			if (retval == -1)
				printf("[error] failed to listen.\n");
			if (DEBUG)
				printf("Waiting for connection\n");

			// Write to pipe 1 to let parent know we are listening
			write(fds1[1], &w, 1);

			// wait for connection
			int fd_connect = accept(fd_server, (struct sockaddr *)0,
									(socklen_t *)0);
			if (DEBUG)
				printf("Connection accepted.\n");

			// Read update from parent on pipe 2
			read(fds2[0], &r, 1);

			// remove sockets and close file descriptors and pipes
			remove(sock);
			close(fd_server);
			close(fd_connect);
			close(fds1[1]);
			close(fds2[0]);

			// job done, time to exit
			exit(0);
		}
		else
		{
			close(fds1[1]); // close the write end of pipe 1
			close(fds2[0]); // close the read end of pipe 2

			// Wait for update from child
			read(fds1[0], &r, 1);

			// create socket
			int fd_client = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd_client < 0)
				printf("[error] failed to open client socket.\n");

			// connect to child address
			retval = connect(fd_client, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to connect.\n");

			// send the buffer over to child (for warm-up)
			retval = send(fd_client, buf, msg_size, MSG_DONTWAIT);

			// send buffer over to child and measure latency
			clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef BYPASS
			retval = bp_sendto(fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);
#else
			retval = syscall(SYS_sendto, fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);
#endif
			clock_gettime(CLOCK_MONOTONIC, &endTime);

			if (retval == -1)
			{
				printf("[error %d] failed to send. %s\n", errno, strerror(errno));
			}

			calc_diff(&diffTime, &endTime, &startTime);
			fprintf(fp, "%d,%d,%ld.%09ld\n", l, msg_size, diffTime.tv_sec, diffTime.tv_nsec);
			fflush(fp);

			// update child so clean-up can happen
			write(fds2[1], &w, 1);

			// do cleanup your-self
			close(fd_client);
			close(fds1[0]);
			close(fds2[1]);

			wait(&status);
		}
	}

#if defined(USE_VMALLOC)
	vfree(buf);
#elif defined(USE_MALLOC)
	free(buf);
#else
	syscall(SYS_munmap, buf, msg_size);
#endif

	return;
}

void recv_bench(int msg_size)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};

	// need to set affinity of parent and child to different cpus
	int retval, forkId, status, l;
	int fds1[2], fds2[2];
	char w = 'b', r;
	char *buf;
	struct sockaddr_un server_addr;

	int recvbuff, retsock, sendbuff, newbuff;
	socklen_t optlen = sizeof(recvbuff);

	cpu_set_t cpuset;
	int prio;

	if (DEBUG)
		printf("recv_bench(%d)\n", msg_size);

	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;

	strncpy(server_addr.sun_path, sock, sizeof(server_addr.sun_path) - 1);

	// create a buffer (this needs to be mmap)
#if defined(USE_VMALLOC)
	buf = (char *)vmalloc(sizeof(char) * msg_size);
#elif defined(USE_MALLOC)
	buf = (char *)malloc(sizeof(char) * msg_size);
#else
	buf = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, sizeof(char) * msg_size,
						  PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	for (l = 0; l < LOOP; l++)
	{
		retval = pipe(fds1);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");
		retval = pipe(fds2);
		if (retval != 0)
			printf("[error] failed to open pipe1.\n");
		forkId = fork();

		if (forkId < 0)
		{
			printf("[error] fork failed.\n");
			return;
		}
		if (forkId > 0)
		{					// parent
			close(fds1[0]); // close the read end of pipe 1
			close(fds2[1]); // close the write end of pipe 2

			// create a socket for server
			int fd_server = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd_server < 0)
				printf("[error] failed to open server socket.\n");

			// bind the socket to a filename so communication can occur
			retval = bind(fd_server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to bind.\n");

			// Listen on the socket
			retval = listen(fd_server, 11);
			if (retval == -1)
				printf("[error] failed to listen.\n");
			if (DEBUG)
				printf("Waiting for connection\n");

			// Write to pipe 1 to let parent know we are listening
			write(fds1[1], &w, 1);

			// wait for connection
			int fd_connect = accept(fd_server, (struct sockaddr *)0,
									(socklen_t *)0);
			if (DEBUG)
				printf("Connection accepted.\n");

			// Read update from parent on pipe 2
			read(fds2[0], &r, 1);

			// recv data from child (for warm-up)
			retval = recv(fd_connect, buf, msg_size, MSG_DONTWAIT);

			// recv data from child and measure latency
			clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef BYPASS
			retval = bp_recvfrom(fd_connect, buf, msg_size, MSG_DONTWAIT, NULL, NULL);
#else
			retval = syscall(SYS_recvfrom, fd_connect, buf, msg_size, MSG_DONTWAIT, NULL, NULL);
#endif
			clock_gettime(CLOCK_MONOTONIC, &endTime);

			if (retval == -1)
			{
				printf("[error %d] failed to send. %s\n", errno, strerror(errno));
			}

			calc_diff(&diffTime, &endTime, &startTime);
			fprintf(fp, "%d,%d,%ld.%09ld\n", l, msg_size, diffTime.tv_sec, diffTime.tv_nsec);
			fflush(fp);

			// update child so clean-up can happen
			write(fds1[1], &w, 1);

			// remove sockets and close file descriptors and pipes
			remove(sock);
			close(fd_server);
			close(fd_connect);
			close(fds1[1]);
			close(fds2[0]);

			wait(&status);
		}
		else
		{
			close(fds1[1]); // close the write end of pipe 1
			close(fds2[0]); // close the read end of pipe 2

			// Wait for update from parent
			read(fds1[0], &r, 1);

			// create socket
			int fd_client = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd_client < 0)
				printf("[error] failed to open client socket.\n");

			// connect to parent address
			retval = connect(fd_client, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
			if (retval == -1)
				printf("[error] failed to connect.\n");

			// write character 'a' in the buffer
			for (int i = 0; i < msg_size; i++)
			{
				buf[i] = i % 93;
			}

			// send data over to parent + one more copy for warm up
			for (l = 0; l < 2; l++)
			{
				retval = syscall(SYS_sendto, fd_client, buf, msg_size, MSG_DONTWAIT, NULL, 0);

				if (retval == -1)
				{
					printf("[error %d] failed to send. %s\n", errno, strerror(errno));
				}
			}

			// tell parent it should start reading
			write(fds2[1], &w, 1);

			// wait for parent
			read(fds1[0], &r, 1);

			// do cleanup your-self
			close(fd_client);
			close(fds1[0]);
			close(fds2[1]);

			// job done, time to exit
			exit(0);
		}
	}

#if defined(USE_VMALLOC)
	vfree(buf);
#elif defined(USE_MALLOC)
	free(buf);
#else
	syscall(SYS_munmap, buf, msg_size);
#endif

	return;
}

struct timespec *forkTime;

void fork_bench(void)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	forkTime = mmap(NULL, sizeof(struct timespec), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int forkId, l, status;

	for (l = 0; l < LOOP; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
		forkId = fork();
		if (forkId == 0)
		{
			clock_gettime(CLOCK_MONOTONIC, forkTime);
			exit(0);
		}
		else if (forkId > 0)
		{
			clock_gettime(CLOCK_MONOTONIC, &endTime);
			wait(&status);
		}
		else
		{
			printf("[error] fork failed.\n");
			fflush(stdout);
		}
		calc_diff(&diffTime, &endTime, &startTime);
		calc_diff(&aveTime, forkTime, &startTime);
		fprintf(fp, "%d,%ld.%09ld,%ld.%09ld\n", l, diffTime.tv_sec, diffTime.tv_nsec, aveTime.tv_sec, aveTime.tv_nsec);
		fflush(fp);
	}

	munmap(forkTime, sizeof(struct timespec));
	return;
}

struct timespec *threadTime;

void *thrdfnc(void *args)
{
	clock_gettime(CLOCK_MONOTONIC, threadTime);
	pthread_exit(0);
}

void thread_bench(void)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	int l, retval;
	pthread_t newThrd;

	threadTime = (struct timespec *)malloc(sizeof(struct timespec));

	for (l = 0; l < LOOP; l++)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
		retval = pthread_create(&newThrd, NULL, thrdfnc, NULL);
		clock_gettime(CLOCK_MONOTONIC, &endTime);

		pthread_join(newThrd, NULL);

		calc_diff(&diffTime, &endTime, &startTime);
		calc_diff(&aveTime, threadTime, &startTime);
		fprintf(fp, "%d,%ld.%09ld,%ld.%09ld\n", l, diffTime.tv_sec, diffTime.tv_nsec, aveTime.tv_sec, aveTime.tv_nsec);
		fflush(fp);
	}

	free(threadTime);
	return;
}

void pagefault_bench(int file_size)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	char *buf;
	int fd, l, i;
	FILE *fp2;
	char *addr;
	char a[file_size];

	for (l = 0; l < LOOP; l++)
	{
		fd = open("tmp_file.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
		if (fd < 0)
		{
			perror("invalid fd in write\n");
			exit(0);
		}

		addr = (char *)syscall(SYS_mmap, (void *)ADDR_HINT, file_size, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		clock_gettime(CLOCK_MONOTONIC, &startTime);
		i = 0;
		while (i < file_size)
		{
			addr[i] = i % 93 + 33;
			i = i + 4096;
			// i++;
		}
		clock_gettime(CLOCK_MONOTONIC, &endTime);

		syscall(SYS_write, fd, addr, file_size);

		close(fd);

		syscall(SYS_munmap, addr, file_size);

		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}
	return;
}

void stack_pagefault_bench(int file_size)
{
	struct timespec startTime = {0, 0}, endTime = {0, 0};
	struct timespec diffTime = {0, 0}, aveTime = {0, 0};
	char *buf;
	int fd, l, i;
	FILE *fp2;
	char *addr;
	char a[file_size];

	for (l = 0; l < LOOP; l++)
	{
		fd = open("tmp_file.txt", O_CREAT | O_WRONLY);
		if (fd < 0)
		{
			printf("invalid fd in write: %d\n", fd);
			exit(0);
		}

		addr = (char *)alloca(file_size * sizeof(long));

		clock_gettime(CLOCK_MONOTONIC, &startTime);
		i = 0;
		while (i < file_size)
		{
			addr[i] = i % 93 + 33;
			i = i + 4096;
		}
		clock_gettime(CLOCK_MONOTONIC, &endTime);

		syscall(SYS_write, fd, addr, file_size);

		close(fd);

		calc_diff(&diffTime, &endTime, &startTime);
		fprintf(fp, "%d,%d,%ld.%09ld\n", l, file_size, diffTime.tv_sec, diffTime.tv_nsec);
		fflush(fp);
	}
	return;
}

//---------------------------------------------------------------------

#ifdef BYPASS
extern void set_bypass_limit(int val);
extern void set_bypass_syscall(int val);
#endif

int main(void)
{
	int file_size, pf_size, fd_count, retval;
	int i = 0, percentage = 0;
	void *addr;

	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(CPU1, &set);
	retval = sched_setaffinity(getpid(), sizeof(set), &set);
	if (retval == -1)
		printf("[error] failed to set processor affinity.\n");
	retval = setpriority(PRIO_PROCESS, 0, -20);
	if (retval == -1)
		printf("[error] failed to set process priority.\n");

	remove("test_file.txt");
	remove("tmp_file.txt");

#ifdef BYPASS
	// set_bypass_limit(50);
	// set_bypass_syscall(1);
#endif

	//*************************************

#ifdef REF_TEST
	printf("Starting reference benchmarks\n");

	fp = fopen("./new_lebench_clock.csv", "w");
	fprintf(fp, "Sr,latency\n");
	fflush(fp);
	clock_bench();
	fclose(fp);

	fp = fopen("./new_lebench_cpu.csv", "w");
	fprintf(fp, "Sr,latency\n");
	fflush(fp);
	cpu_bench();
	fclose(fp);

	fp = fopen("./new_lebench_getppid.csv", "w");
	fprintf(fp, "Sr,latency\n");
	fflush(fp);
	getppid_bench();
	fclose(fp);
	printf("Reference benchmarks done\n");
#endif

	//*************************************

#ifdef THREAD_TEST
	printf("Running Thread Create benchmarks\n");

	fp = fopen("./new_lebench_thread.csv", "w");

	fprintf(fp, "Sr,LatencyParent,LatencyChild\n");
	fflush(fp);

	thread_bench();

	printf("Running thread test 100 %% done\n");
	fflush(stdout);
	fclose(fp);
#endif

	//*************************************

#ifdef FORK_TEST
	fp = fopen("./new_lebench_fork.csv", "w");

	fprintf(fp, "Sr,LatencyParent,LatencyChild\n");
	fflush(fp);

	fork_bench();

	printf("Running fork test 100 %% done\n");
	fflush(stdout);
	fclose(fp);
#endif

	//*************************************

#ifdef SEND_TEST
	fp = fopen("./new_lebench_send.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	send_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		send_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running send test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef RECV_TEST
	fp = fopen("./new_lebench_recv.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	recv_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		recv_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running recv test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef WRITE_TEST
	fp = fopen("./new_lebench_write.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	write_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		write_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running write test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef READ_TEST
	fp = fopen("./new_lebench_read.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	file_size = 1;
	read_bench(file_size);
	file_size = 0;
	i = 0;
	while (file_size < MAX_SIZE)
	{
		file_size = file_size + STEP;
		read_bench(file_size);
		i++;
		if (i > CENT)
		{
			i = 0;
			percentage = (file_size * 100) / MAX_SIZE;
			printf("Running read test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef PF_TEST
	fp = fopen("./new_lebench_pagefault.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	pf_size = 0;
	i = 0;
	while (pf_size < PF_MAX_SIZE)
	{
		pf_size = pf_size + PF_STEP;
		pagefault_bench(pf_size);
		i++;
		if (i > PF_CENT)
		{
			i = 0;
			percentage = (pf_size * 100) / (PF_MAX_SIZE);
			printf("Running pagefault test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif

	//*************************************

#ifdef ST_PF_TEST
	fp = fopen("./new_lebench_stackpagefault.csv", "w");

	fprintf(fp, "Sr,Size,Latency\n");
	fflush(fp);
	pf_size = 0;
	i = 0;
	while (pf_size < PF_MAX_SIZE)
	{
		pf_size = pf_size + PF_STEP;
		pagefault_bench(pf_size);
		i++;
		if (i > PF_CENT)
		{
			i = 0;
			percentage = (pf_size * 100) / (PF_MAX_SIZE);
			printf("Running stack pagefault test %d %% done\n", percentage);
			fflush(stdout);
		}
	}

	fclose(fp);
#endif
}
