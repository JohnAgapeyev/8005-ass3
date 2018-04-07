/*
 * HEADER FILE: network.h - Main networking calls and data
 *
 * PROGRAM: 8005-ass3
 *
 * DATE: April 7 2018
 *
 * FUNCTIONS:
 * void network_init(void);
 * void network_cleanup(void);
 * void process_packet(const unsigned char * const buffer, const size_t bufsize, struct client *src);
 * void startServer(void);
 * size_t addClient(int sock);
 * void initClientStruct(struct client *newClient, int sock);
 * void *eventLoop(void *epollfd);
 * void handleIncomingConnection(const int listen_sock, const int index);
 * void handleSocketError(struct client *entry);
 * void handleIncomingPacket(struct client *src);
 * void establish_forwarding_rule(const long listen_port, const char *addr, const char *output_port);
 *
 * VARIABLES:
 * extern struct client **clientList - A list of all clients and connections
 * extern size_t clientCount - The current number of clients in the list
 * extern size_t clientCount - The current number of allocated client entries
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

struct client {
    int local;
    int remote;
    bool enabled;
    pthread_mutex_t *lock;
    int pipes[2];
};

extern struct client **clientList;
extern size_t clientCount;
extern size_t clientMax;

void network_init(void);
void network_cleanup(void);
void process_packet(const unsigned char * const buffer, const size_t bufsize, struct client *src);
void startServer(void);
size_t addClient(int sock);
void initClientStruct(struct client *newClient, int sock);
void *eventLoop(void *epollfd);
void handleIncomingConnection(const int listen_sock, const int index);
void handleSocketError(struct client *entry);
void handleIncomingPacket(struct client *src);
void establish_forwarding_rule(const long listen_port, const char *addr, const char *output_port);

#endif
