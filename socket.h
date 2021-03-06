/*
 * HEADER FILE: socket.h - Socket calls and their wrappers
 *
 * PROGRAM: 7005-asn4
 *
 * DATE: Dec. 2, 2017
 *
 * FUNCTIONS:
 * int createSocket(int domain, int type, int protocol);
 * void setNonBlocking(const int sock);
 * void bindSocket(const int sock, const unsigned short port);
 * int establishConnection(const char *address, const char *port);
 * void forward_traffic(const int in, const int out, const struct client *const client);
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#ifndef SOCKET_H
#define SOCKET_H

#include "network.h"

int createSocket(int domain, int type, int protocol);
void setNonBlocking(const int sock);
void bindSocket(const int sock, const unsigned short port);
int establishConnection(const char *address, const char *port);
void forward_traffic(const int in, const int out, const struct client *const client);

#endif
