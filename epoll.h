/*
 * HEADER FILE: epoll.h - Epoll setup and wrappers
 *
 * PROGRAM: 7005-asn4
 *
 * DATE: Dec. 2, 2017
 *
 * FUNCTIONS:
 * int createEpollFd(void);
 * void addEpollSocket(const int epollfd, const int sock, struct epoll_event *ev);
 * int waitForEpollEvent(const int epollfd, struct epoll_event *events);
 * size_t singleEpollReadInstance(const int sock, unsigned char *buffer, const size_t bufSize);
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>

#define MAX_EPOLL_EVENTS 20

int createEpollFd(void);
void addEpollSocket(const int epollfd, const int sock, struct epoll_event *ev);
int waitForEpollEvent(const int epollfd, struct epoll_event *events);

#endif

