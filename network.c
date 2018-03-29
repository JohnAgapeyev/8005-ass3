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
int efd;

uint64_t epoll_mask = 0xffffff;

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
    clientList = checked_calloc(100, sizeof(struct client *));
    clientCount = 1;
    clientMax = 100;
    pthread_mutex_init(&clientLock, NULL);
    efd = createEpollFd();
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
            close(clientList[i]->pipes[0]);
            close(clientList[i]->pipes[1]);
            free(clientList[i]);
        }
    }
    pthread_mutex_destroy(&clientLock);
    free(clientList);
    close(efd);
}

void establish_forwarding_rule(const long listen_port, const char *restrict addr, const char *restrict output_port) {
    unsigned int sock = createSocket(AF_INET, SOCK_STREAM, 0);

    setNonBlocking(sock);

    bindSocket(sock, listen_port);
    listen(sock, SOMAXCONN);

    int remote = establishConnection(addr, output_port);

    setNonBlocking(sock);

    unsigned int index = addClient(remote);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ev.data.u64 = ((uint64_t) sock << 24ul) + ((uint64_t) index << 48ul);

    addEpollSocket(efd, sock, &ev);
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

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    cpu_set_t cpus;

    pthread_t threads[core_count - 1];
    for (size_t i = 0; i < core_count - 1; ++i) {
        CPU_ZERO(&cpus);
        CPU_SET(i % core_count, &cpus);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
        pthread_create(&threads[i], &attr, eventLoop, &efd);
    }
    pthread_attr_destroy(&attr);

    eventLoop(&efd);

    for (size_t i = 0; i < core_count - 1; ++i) {
        pthread_kill(threads[i], SIGKILL);
        pthread_join(threads[i], NULL);
    }
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
            if (unlikely(eventList[i].events & EPOLLERR || eventList[i].events & EPOLLHUP)) {
                handleSocketError(eventList[i].data.ptr);
            } else {
                if (likely(eventList[i].events & EPOLLIN)) {
                    if ((eventList[i].data.u64 & epoll_mask) != 0) {
                        //Regular read connection
                        struct client *client;
                        if (eventList[i].data.u64 & 1) {
                            client = (struct client *) (eventList[i].data.u64 - 1);
                            forward_traffic(client->remote, client->local, client);
                        } else {
                            client = (struct client *) (eventList[i].data.u64);
                            forward_traffic(client->local, client->remote, client);
                        }
                    } else {
                        //Shift fd back, and zero the index portion
                        unsigned int listen_sock = (eventList[i].data.u64 >> 24) & epoll_mask;
                        unsigned int index = eventList[i].data.u64 >> 48;
                        handleIncomingConnection(listen_sock, index);
                    }
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
    newClient->local = 0;
    newClient->remote = sock;
    newClient->lock = checked_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(newClient->lock, NULL);
    newClient->enabled = true;
    if (pipe(newClient->pipes) < 0) {
        fatal_error("pipe");
    }
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
void handleIncomingConnection(const int listen_sock, const int index) {
    int local = accept(listen_sock, NULL, NULL);
    if (local == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            //No incoming connections, ignore the error
            return;
        }
        fatal_error("accept");
    }

    setNonBlocking(local);

    pthread_mutex_lock(&clientLock);
    struct client *newClientEntry = clientList[index];
    pthread_mutex_unlock(&clientLock);

    newClientEntry->local = local;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ev.data.u64 = ((uintptr_t) newClientEntry);

    addEpollSocket(efd, newClientEntry->local, &ev);

    ev.data.u64 = ((uintptr_t) newClientEntry) + 1;

    addEpollSocket(efd, newClientEntry->remote, &ev);
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
    int sock = (entry) ? entry->local: entry->remote;
    fprintf(stderr, "Disconnection/error on socket %d\n", sock);

    //Don't need to deregister socket from epoll
    close(sock);

    entry->enabled = false;

    pthread_mutex_unlock(&clientLock);
}

