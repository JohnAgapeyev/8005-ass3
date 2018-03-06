#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include "network.h"
#include "epoll.h"
#include "socket.h"
#include "macro.h"
#include "main.h"

struct client **clientList;
size_t clientCount;
size_t clientMax;
unsigned short port;
int listenSock;

pthread_mutex_t clientLock;

/*
 * FUNCTION: network_init
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
 * void network_init(void);
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Initializes network state for the application
 */
void network_init(void) {
    clientList = checked_calloc(10000, sizeof(struct client *));
    clientCount = 1;
    clientMax = 10000;
    pthread_mutex_init(&clientLock, NULL);
}

/*
 * FUNCTION: network_cleanup
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
 * void network_cleanup(void);
 *
 * RETURNS:
 * void
 */
void network_cleanup(void) {
    for (size_t i = 0; i < clientMax; ++i) {
        if (clientList[i]) {
            pthread_mutex_destroy(clientList[i]->lock);
            free(clientList[i]->lock);
            free(clientList[i]);
        }
    }
    pthread_mutex_destroy(&clientLock);
    free(clientList);
}

/*
 * FUNCTION: process_packet
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
 * void process_packet(const unsigned char * const buffer, const size_t bufsize, struct client *src);
 *
 * PARAMETERS:
 * const unsigned char *const buffer - The buffer containing the buffer
 * const size_T bufsize - The size of the packet buffer
 * struct client *src - The client struct of who sent the packet
 *
 * RETURNS:
 * void
 */
void process_packet(const unsigned char * const buffer, const size_t bufsize, struct client *src) {
    debug_print("Received packet of size %zu\n", bufsize);
    debug_print_buffer("Raw hex output: ", buffer, bufsize);

#ifndef NDEBUG
    debug_print("\nText output: ");
    for (size_t i = 0; i < bufsize; ++i) {
        fprintf(stderr, "%c", buffer[i]);
    }
    fprintf(stderr, "\n");
#endif
}

/*
 * FUNCTION: startServer
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
 * void startServer(const int inputFD)
 *
 * PARAMETERS:
 * const int inputFD - The file descriptor to read from in order to get packet data to send
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Performs similar functions to startClient, except for the inital connection.
 */
void startServer(void) {
    const size_t core_count = sysconf(_SC_NPROCESSORS_ONLN);
    network_init();

    int efd = createEpollFd();

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ev.data.ptr = NULL;

    addEpollSocket(efd, listenSock, &ev);

    setNonBlocking(listenSock);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    cpu_set_t cpus;

    pthread_t threads[core_count];
    for (size_t i = 0; i < core_count; ++i) {
        CPU_ZERO(&cpus);
        CPU_SET(i % core_count, &cpus);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
        pthread_create(&threads[i], &attr, eventLoop, &(int){i % core_count});
    }
    pthread_attr_destroy(&attr);

    eventLoop(&(int){core_count});

    for (size_t i = 0; i < core_count; ++i) {
        pthread_kill(threads[i], SIGKILL);
        pthread_join(threads[i], NULL);
    }

    network_cleanup();
}

/*
 * FUNCTION: eventLoop
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
 * void *eventLoop(void *epollfd)
 *
 * PARAMETERS:
 * void *epollfd - The address of an epoll descriptor
 *
 * RETURNS:
 * void * - Required by pthread interface, ignored.
 *
 * NOTES:
 * Both client and server read threads run this function.
 */
void *eventLoop(void *epollfd) {
    int efd = *((int *)epollfd);

    struct epoll_event *eventList = checked_calloc(MAX_EPOLL_EVENTS, sizeof(struct epoll_event));

    while (isRunning) {
        int n = waitForEpollEvent(efd, eventList);
        //n can't be -1 because the handling for that is done in waitForEpollEvent
        assert(n != -1);
        for (int i = 0; i < n; ++i) {
            if (unlikely(eventList[i].events & EPOLLERR || eventList[i].events & EPOLLHUP
                        || eventList[i].events & EPOLLRDHUP)) {
                handleSocketError(eventList[i].data.ptr);
            } else {
                if (likely(eventList[i].events & EPOLLIN)) {
                    if (eventList[i].data.ptr) {
                        //Regular read connection
                        handleIncomingPacket(eventList[i].data.ptr);
                    } else {
                        //Null data pointer means listen socket has incoming connection
                        handleIncomingConnection(efd);
                    }
                }
                if (likely(eventList[i].events & EPOLLOUT)) {
                    //unsigned char data[MAX_INPUT_SIZE];
                }
            }
        }
    }
    free(eventList);
    return NULL;
}

/*
 * FUNCTION: addClient
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
 * size_t addClient(int sock)
 *
 * PARAMETERS:
 * int sock - The new client's socket
 *
 * RETURNS:
 * size_t - The index of the newly created client entry
 */
size_t addClient(int sock) {
    pthread_mutex_lock(&clientLock);
    bool foundEntry = false;
    for (size_t i = 0; i < clientMax; ++i) {
        if (clientList[i] && clientList[i]->enabled == false) {
            initClientStruct(clientList[i], sock);
            assert(clientList[i]->enabled);
            foundEntry = true;
            pthread_mutex_unlock(&clientLock);
            return i;
        }
        if (clientList[i] == NULL) {
            clientList[i] = checked_malloc(sizeof(struct client));
            initClientStruct(clientList[i], sock);
            assert(clientList[i]->enabled);
            foundEntry = true;
            ++clientCount;
            pthread_mutex_unlock(&clientLock);
            return i;
        }
    }
    if (!foundEntry) {
        clientList = checked_realloc(clientList, sizeof(struct client *) * clientMax * 2);
        memset(clientList + clientMax, 0, sizeof(struct client *) * clientMax);
        clientList[clientMax] = checked_malloc(sizeof(struct client));
        initClientStruct(clientList[clientMax], sock);
        clientMax *= 2;
    }
    ++clientCount;
    size_t result = clientCount - 2;
    pthread_mutex_unlock(&clientLock);
    //Subtract 2: 1 for incremented client count, 1 for dummy value
    return result;
}

/*
 * FUNCTION: initClientStruct
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
 * void initClientStruct(struct client *newClient, int sock)
 *
 * PARAMETERS:
 * struct client *newClient - A pointer to the new client's struct
 * int sock - The new client's socket
 *
 * RETURNS:
 * void
 */
void initClientStruct(struct client *newClient, int sock) {
    newClient->socket = sock;
    newClient->lock = checked_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(newClient->lock, NULL);
    newClient->enabled = true;
}

/*
 * FUNCTION: handleIncomingConnection
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
 * void handleIncomingConnection(const int efd);
 *
 * PARAMETERS:
 * const int efd - The epoll descriptor that had the event
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Adds an incoming connection to the client list, and initiates the handshake.
 */
void handleIncomingConnection(const int efd) {
    for(;;) {
        int sock = accept(listenSock, NULL, NULL);
        if (sock == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                //No incoming connections, ignore the error
                break;
            }
            fatal_error("accept");
        }

        setNonBlocking(sock);

        size_t newClientIndex = addClient(sock);
        pthread_mutex_lock(&clientLock);
        struct client *newClientEntry = clientList[newClientIndex];
        pthread_mutex_unlock(&clientLock);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
        ev.data.ptr = newClientEntry;

        addEpollSocket(efd, sock, &ev);
    }
}

/*
 * FUNCTION: handleSocketError
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
 * void handleSocketError(const int sock);
 *
 * PARAMETERS:
 * const int sock - The socket that had the error
 *
 * RETURNS:
 * void
 */
void handleSocketError(struct client *entry) {
    pthread_mutex_lock(&clientLock);
    int sock = (entry) ? entry->socket : listenSock;
    fprintf(stderr, "Disconnection/error on socket %d\n", sock);

    //Don't need to deregister socket from epoll
    close(sock);

    entry->enabled = false;

    pthread_mutex_unlock(&clientLock);
}

/*
 * FUNCTION: handleIncomingPacket
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
 * void handleIncomingPacket(struct client *src);
 *
 * PARAMETERS:
 * struct client *src - The source of the incoming packet
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Handles the staggered and full read, before passing the packet off.
 */
void handleIncomingPacket(struct client *src) {
    const int sock = src->socket;
    //unsigned char buffer[MAX_PACKET_SIZE];
    for (;;) {
    }
}

