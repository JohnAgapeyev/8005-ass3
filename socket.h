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
 * size_t readNBytes(const int sock, unsigned char *buf, size_t bufsize);
 * void rawSend(const int sock, const unsigned char *buffer, size_t bufSize);
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#ifndef SOCKET_H
#define SOCKET_H

int createSocket(int domain, int type, int protocol);
void setNonBlocking(const int sock);
void bindSocket(const int sock, const unsigned short port);
int establishConnection(const char *address, const char *port);
ssize_t readNBytes(const int sock, unsigned char *buf, size_t bufsize);
void rawSend(const int sock, const unsigned char *buffer, size_t bufSize);

#endif
