#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

struct client {
    int socket_1;
    int socket_2;
    bool enabled;
    pthread_mutex_t *lock;
    int pipes[2];
};

extern struct client **clientList;
extern size_t clientCount;
extern unsigned short port;
extern int listenSock;

void network_init(void);
void network_cleanup(void);
void process_packet(const unsigned char * const buffer, const size_t bufsize, struct client *src);
void startServer(void);
size_t addClient(int sock);
void initClientStruct(struct client *newClient, int sock);
void *eventLoop(void *epollfd);
void handleIncomingConnection(const int efd);
void handleSocketError(struct client *entry);
void handleIncomingPacket(struct client *src);
void establish_forwarding_rule(const long listen_port, const char *addr, const long output_port);

#endif
