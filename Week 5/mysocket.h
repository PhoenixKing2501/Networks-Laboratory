#pragma once

#ifndef _MYSOCKET_H_
#define _MYSOCKET_H_

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define SOCK_MyTCP 1000

int my_socket(int __domain, int __type, int __protocol);
int my_listen(int __fd, int __n);
int my_accept(int __fd, __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len);
int my_connect(int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);
int my_bind(int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);
ssize_t my_recv(int __fd, void *__buf, size_t __n, int __flags);
ssize_t my_send(int __fd, const void *__buf, size_t __n, int __flags);
int my_close(int __fd);

#endif // _MYSOCKET_H_
