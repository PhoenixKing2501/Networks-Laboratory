#include "mysocket.h"
#define MAX_MSG_SIZE 5000
#define MAX_MSG 10
#define CHUNK 250lu

typedef struct _msg
{
	void *data;
	size_t size;
} MSG;

void deletemsg(MSG *msg)
{
	free(msg->data);
	free(msg);
}

/* --------Circular Queue implementation-------- */

typedef struct _cqueue
{
	MSG *cqueue[MAX_MSG + 1];
	int st;
	int end;
} *CQueue;

void initqueue(CQueue Q)
{
	Q->st = 0;
	Q->end = 0;
	for (int i = 0; i < MAX_MSG + 1; i++)
	{
		Q->cqueue[i] = NULL;
	}
}
bool isempty(CQueue Q)
{
	return Q->st == Q->end;
}

bool isfull(CQueue Q)
{
	return (Q->end + 1) % (MAX_MSG + 1) == Q->st;
}

void push(CQueue Q, MSG *msg)
{
	if (isfull(Q))
	{
		return;
	}

	// duplicate msg
	MSG *dup = malloc(sizeof(MSG));
	dup->size = msg->size;
	dup->data = malloc(msg->size);
	memcpy(dup->data, msg->data, msg->size);

	Q->cqueue[Q->end] = dup;
	Q->end = (Q->end + 1) % (MAX_MSG + 1);
	Q->cqueue[Q->end] = NULL;
}

void pop(CQueue Q)
{
	if (isempty(Q))
	{
		return;
	}

	deletemsg(Q->cqueue[Q->st]);
	Q->cqueue[Q->st] = NULL;
	Q->st = (Q->st + 1) % (MAX_MSG + 1);
}

MSG *front(CQueue Q)
{
	if (isempty(Q))
	{
		return NULL;
	}

	// duplicate msg
	MSG *dup = malloc(sizeof(MSG));
	dup->size = Q->cqueue[Q->st]->size;
	dup->data = malloc(dup->size);
	memcpy(dup->data, Q->cqueue[Q->st]->data, dup->size);

	return dup;
}

void freeCQ(CQueue Q)
{
	for (int i = 0; i < MAX_MSG + 1; i++)
	{
		if (Q->cqueue[i] != NULL)
		{
			deletemsg(Q->cqueue[i]);
		}
	}
}

/*-----------------------------------------------------*/

CQueue sendTable, recvTable;
pthread_t sendThread, recvThread;
pthread_mutex_t sendMutex, recvMutex;
pthread_cond_t sendCond1, sendCond2, recvCond1, recvCond2;

void send_helper(int sockfd, MSG *msg)
{
	size_t sent = 0;
	ssize_t ret;
	uint8_t sz;
	while (sent < msg->size)
	{
		size_t tosend = msg->size - sent < CHUNK ? msg->size - sent : CHUNK;
		sz = (uint8_t)tosend;
		ret = send(sockfd, &sz, sizeof(sz), 0);
		if (ret < 0)
		{
			perror("send");
			exit(1);
		}

		while (tosend > 0)
		{
			ret = send(sockfd, (char *)msg->data + sent, tosend, 0);
			if (ret < 0)
			{
				perror("send");
				exit(1);
			}
			tosend -= ret;
			sent += ret;
		}
	}
	sz = 0;
	ret = send(sockfd, &sz, sizeof(sz), 0);
	if (ret < 0)
	{
		perror("send");
		exit(1);
	}
}

MSG *recv_helper(int sockfd)
{
	char buf[MAX_MSG_SIZE];
	size_t size = 0;

	ssize_t ret;
	uint8_t sz;
	while (true)
	{
		ret = recv(sockfd, &sz, sizeof(sz), 0);
		if (ret < 0)
		{
			perror("recv");
			exit(1);
		}
		else if (ret == 0)
		{
			break;
		}

		size_t torecv = sz;
		if (torecv == 0)
		{
			break;
		}

		while (torecv > 0)
		{
			ret = recv(sockfd, buf + size, torecv, 0);
			if (ret < 0)
			{
				perror("recv");
				exit(1);
			}
			else if (ret == 0)
			{
				break;
			}
			torecv -= ret;
			size += ret;
		}
	}

	MSG *msg = malloc(sizeof(MSG));
	msg->data = malloc(size);
	memcpy(msg->data, buf, size);
	msg->size = size;

	return msg;
}

void *sendThreadFunc(void *arg)
{
	int sockfd = *(int *)arg;
	free(arg);

	while (true)
	{
		pthread_mutex_lock(&sendMutex);
		while (isempty(sendTable))
		{
			pthread_cond_wait(&sendCond1, &sendMutex);
		}

		MSG *msg = front(sendTable);
		pop(sendTable);
		pthread_mutex_unlock(&sendMutex);
		pthread_cond_signal(&sendCond2);

		send_helper(sockfd, msg);
		// puts("send");

		deletemsg(msg);
	}

	return NULL;
}

void *recvThreadFunc(void *arg)
{
	int sockfd = *(int *)arg;
	free(arg);

	while (true)
	{
		pthread_mutex_lock(&recvMutex);
		while (isfull(recvTable))
		{
			pthread_cond_wait(&recvCond1, &recvMutex);
		}

		MSG *msg = recv_helper(sockfd);
		// puts("recv");

		push(recvTable, msg);
		deletemsg(msg);
		pthread_mutex_unlock(&recvMutex);
		pthread_cond_signal(&recvCond2);
	}

	return NULL;
}

void init_connection(int sockfd)
{
	int *a = malloc(sizeof(int));
	*a = sockfd;
	int *b = malloc(sizeof(int));
	*b = sockfd;

	pthread_mutex_init(&sendMutex, NULL);
	pthread_mutex_init(&recvMutex, NULL);
	pthread_cond_init(&sendCond1, NULL);
	pthread_cond_init(&sendCond2, NULL);
	pthread_cond_init(&recvCond1, NULL);
	pthread_cond_init(&recvCond2, NULL);
	pthread_create(&sendThread, NULL, sendThreadFunc, a);
	pthread_create(&recvThread, NULL, recvThreadFunc, b);

	sendTable = malloc(sizeof(*sendTable));
	recvTable = malloc(sizeof(*recvTable));

	initqueue(sendTable);
	initqueue(recvTable);
	puts("connection initialized");
}

int my_socket(int domain, int type, int protocol)
{
	return socket(domain, (type == SOCK_MyTCP ? SOCK_STREAM : type), protocol);
}

int my_bind(int sockfd, __CONST_SOCKADDR_ARG addr, socklen_t addrlen)
{
	return bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog)
{
	return listen(sockfd, backlog);
}

int my_accept(int sockfd, __SOCKADDR_ARG addr, socklen_t *__restrict addrlen)
{
	int newsockfd = accept(sockfd, addr, addrlen);
	init_connection(newsockfd);
	return newsockfd;
}

int my_connect(int sockfd, __CONST_SOCKADDR_ARG addr, socklen_t addrlen)
{
	init_connection(sockfd);
	return connect(sockfd, addr, addrlen);
}

ssize_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
	// check if recvTable is empty

	pthread_mutex_lock(&recvMutex);
	while (isempty(recvTable))
	{
		pthread_cond_wait(&recvCond2, &recvMutex);
	}

	MSG *msg = front(recvTable);
	pop(recvTable);
	pthread_mutex_unlock(&recvMutex);
	pthread_cond_signal(&recvCond1);

	len = msg->size < len ? msg->size : len;
	memcpy(buf, msg->data, len);
	deletemsg(msg);

	return len;
}

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags)
{
	// check if sendTable is full

	pthread_mutex_lock(&sendMutex);
	while (isfull(sendTable))
	{
		pthread_cond_wait(&sendCond2, &sendMutex);
	}

	MSG msg = {.size = len, .data = malloc(len)};
	memcpy(msg.data, buf, len);
	push(sendTable, &msg);
	free(msg.data);

	pthread_mutex_unlock(&sendMutex);
	pthread_cond_signal(&sendCond1);

	return len;
}

int my_close(int fd)
{
	sleep(5);
	pthread_cancel(sendThread);
	pthread_cancel(recvThread);
	pthread_join(sendThread, NULL);
	pthread_join(recvThread, NULL);

	freeCQ(sendTable);
	freeCQ(recvTable);

	return close(fd);
}
