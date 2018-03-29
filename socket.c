/*
 * SOURCE FILE: socket.c - Implementation of functions declared in socket.h
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
#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include "socket.h"
#include "network.h"
#include "macro.h"

/*
 * FUNCTION: createSocket
 *
 * DATE:
 * Dec. 2, 2017
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * int createSocket(int domain, int type, int protocol);
 *
 * PARAMETERS:
 * int domain - The socket domain to be created for
 * int type - The type of socket to create
 * int protocol - The protocol of the socket
 *
 * RETURNS:
 * int - The socket file descriptor that was created
 */
int createSocket(int domain, int type, int protocol) {
    int sock;
    if ((sock = socket(domain, type, protocol)) == -1) {
        fatal_error("socket");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        fatal_error("SO_REUSEADDR");
    }
    return sock;
}

/*
 * FUNCTION: setNonBlocking
 *
 * DATE:
 * Dec. 2, 2017
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * void setNonBlocking(const int sock);
 *
 * PARAMETERS:
 * const int sock - The socket to set to non blocking mode
 *
 * RETURNS:
 * void
 */
void setNonBlocking(const int sock) {
    int flags;
    if ((flags = fcntl(sock, F_GETFL, 0)) == -1){
        fatal_error("fcntl get");
    }
    flags |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags) == -1) {
        fatal_error("fcntl set");
    }
}

/*
 * FUNCTION: bindSocket
 *
 * DATE:
 * Dec. 2, 2017
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * void bindSocket(const int sock, const unsigned short port);
 *
 * PARAMETERS:
 * const int sock - The socket to bind with
 * const unsigned short port - The port to bind
 *
 * RETURNS:
 * void
 */
void bindSocket(const int sock, const unsigned short port) {
    struct sockaddr_in myAddr;
    memset(&myAddr, 0, sizeof(struct sockaddr_in));
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(port);
    myAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *) &myAddr, sizeof(struct sockaddr_in)) == -1) {
        fatal_error("bind");
    }
}

/*
 * FUNCTION: establishConnection
 *
 * DATE:
 * Dec. 2, 2017
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * int establishConnection(const char *address, const char *port);
 *
 * PARAMETERS:
 * const char *address - A string containing the domain name or ip address of the desired host
 * const char *port - A string containing the port number to connect to
 *
 * RETURNS:
 * int - The socket that is connected to the given address and port
 */
int establishConnection(const char *address, const char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_INET;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags = (AI_ADDRCONFIG | AI_V4MAPPED);

    struct addrinfo *result;
    int e;
    if ((e = getaddrinfo(address, port, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e));
        exit(EXIT_FAILURE);
    }

    struct addrinfo *rp;
    int sock;
    for (rp = result; rp; rp = rp->ai_next) {
        if ((sock = createSocket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
            continue;
        }
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sock);
    }

    if (!rp) {
        fprintf(stderr, "Unable to connect\n");
        return -1;
    }

    freeaddrinfo(result);
    return sock;
}

/*
 * FUNCTION: readNBytes
 *
 * DATE:
 * Dec. 2, 2017
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * size_t readNBytes(const int sock, unsigned char *buf, size_t bufsize);
 *
 * PARAMETERS:
 * const int sock - The socket to read from
 * unsigned char *buf - The buffer to write to
 * size_t bufsize - The size of the given buffer
 *
 * RETURNS:
 * size_t - The amount that was read
 */
ssize_t readNBytes(const int sock, unsigned char *buf, size_t bufsize) {
    const size_t origBufSize = bufsize;
    for (;;) {
        if (bufsize == 0) {
            break;
        }
        const int n = recv(sock, buf, bufsize, 0);
        if (n == -1) {
            switch(errno) {
                case EAGAIN:
                    return origBufSize - bufsize;
                case EBADF:
                case ENOTSOCK:
                    //break;
                default:
                    perror("Socket read");
                    break;
            }
            return -1;
        }
        if (n == 0) {
            //No more data to read, so do nothing
            return origBufSize - bufsize;
        }
        assert((size_t) n <= bufsize);
        bufsize -= n;
        buf += n;
    }
    return origBufSize - bufsize;
}

/*
 * FUNCTION: rawSend
 *
 * DATE:
 * Dec. 2, 2017
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * void rawSend(const int sock, const unsigned char *buffer, size_t bufSize);
 *
 * PARAMETERS:
 * const int sock - The socket to send data over
 * unsigned char *buf - The buffer to send from
 * size_t bufsize - The size of the data to send
 *
 * RETURNS:
 * void
 */
void rawSend(const int sock, const unsigned char *buffer, size_t bufSize) {
    ssize_t n;
start:
    if ((n = send(sock, buffer, bufSize, MSG_NOSIGNAL)) == -1) {
        switch(errno) {
            case EAGAIN:
                //Intentional fallthrough
            case EINTR:
                goto start;
                break;
            case EPIPE:
            case ECONNRESET:
                //Other end has left us, do nothing
                return;
            default:
                //fatal_error("Socket send");
                perror("Socket send");
                break;
        }
    }
    assert(n >= 0);
    if ((size_t) n < bufSize) {
        //Didn't send all the data, try again
        bufSize -= n;
        buffer += n;
        goto start;
    }
}

void forward_traffic(const int in, const int out, const struct client *const client) {
    for (;;) {
        int n = splice(in, NULL, client->pipes[1], NULL, USHRT_MAX, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (n == -1) {
            if (errno == EAGAIN) {
                return;
            } else {
                fatal_error("splice1");
            }
        } else if (n == 0) {
            return;
        } else {
            //Don't need non_blocking because the only blocking would be sending, which should be blocked on
            int x;
resend:
            x = splice(client->pipes[0], NULL, out, NULL, USHRT_MAX, SPLICE_F_MOVE | SPLICE_F_MORE | SPLICE_F_NONBLOCK);
            if (x == -1) {
                fatal_error("splice2");
            } else if (x == 0) {
                //Input pipe is empty
                continue;
            } else if (x == n) {
                //Sent all the data that was read
                continue;
            } else {
                //x < n
                n -= x;
                goto resend;
            }
        }
    }
}
