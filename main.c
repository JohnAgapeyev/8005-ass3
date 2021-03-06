/*
 * SOURCE FILE: main.c - Implementation of functions declared in main.h
 *
 * PROGRAM: 7005-asn4
 *
 * DATE: Dec. 2, 2017
 *
 * FUNCTIONS:
 * static void sighandler(int signo);
 * static void parse_config_file(void);
 * void debug_print_buffer(const char *prompt, const unsigned char *buffer, const size_t size);
 * void *checked_malloc(const size_t size);
 * void *checked_calloc(const size_t nmemb, const size_t size);
 * void *checked_realloc(void *ptr, const size_t size);
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include "main.h"
#include "macro.h"
#include "socket.h"
#include "network.h"

static void sighandler(int signo);
static void parse_config_file(void);

/*
 * FUNCTION: main
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
 * int main(void)
 *
 * RETURNS:
 * int - The application return code
 *
 * NOTES:
 * Starts client/server from here
 */
int main(void) {
    isRunning = ATOMIC_VAR_INIT(1);

    struct sigaction sigHandleList = {.sa_handler=sighandler};
    sigaction(SIGINT,&sigHandleList,0);
    sigaction(SIGHUP,&sigHandleList,0);
    sigaction(SIGQUIT,&sigHandleList,0);
    sigaction(SIGTERM,&sigHandleList,0);

    network_init();
    parse_config_file();
    startServer();
    network_cleanup();

    return EXIT_SUCCESS;
}

/*
 * FUNCTION: parse_config_file
 *
 * DATE:
 * April 7 2018
 *
 * DESIGNER:
 * John Agapeyev
 *
 * PROGRAMMER:
 * John Agapeyev
 *
 * INTERFACE:
 * void parse_config_file(void);
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Config file is hardcoded to be forward.conf in current directory.
 * All rules are CSV, each line is new rule
 * Format is [input port],[output address],[output port]
 * The output port is optional, and will default to the input port when none is provided
 */
void parse_config_file(void) {
    const char *delim = ",\n";
    FILE *fp = fopen("forward.conf", "r");
    if (fp == NULL) {
        fatal_error("forward.conf could not be located");
    }

    char buffer[1025];
    char output_address[1025];
    long listen_port;
    char output_port[1025];
    while(fgets(buffer, 1024, fp)) {
        char *contents = strtok(buffer, delim);
        if (contents == NULL) {
            continue;
        }
        listen_port = strtol(contents, NULL, 10);
        if (errno == ERANGE) {
            fatal_error("Invalid port in config file");
        }
        contents = strtok(NULL, delim);
        if (contents == NULL) {
            fprintf(stderr, "Invalid rule format in config file\n");
            continue;
        }
        strncpy(output_address, contents, 1025);
        contents = strtok(NULL, delim);
        if (contents == NULL) {
            printf("Output port not specified, defaulting to listen port\n");
            sprintf(output_port, "%ld", listen_port);
        } else {
            strncpy(output_port, contents, 1025);
        }

        printf("Adding forwarding on port %ld to %s:%s\n", listen_port, output_address, output_port);
        establish_forwarding_rule(listen_port, output_address, output_port);
    }
}

/*
 * FUNCTION: sighandler
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
 * void sighandler(int signo)
 *
 * PARAMETERS:
 * int signo - The signal number received
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Sets isRunning to 0 to terminate program gracefully in the event of SIGINT or other
 * user sent signals.
 */
void sighandler(int signo) {
    (void)(signo);
    isRunning = 0;
}

/*
 * FUNCTION: debug_print_buffer
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
 * void debug_print_buffer(const char *prompt, const unsigned char *buffer, const size_t size);
 *
 * PARAMETERS:
 * const char *prompt - The prompt to show for debug purposes
 * const unsigned char *buffer - The buffer to print out
 * const size_t size - The size of the buffer
 *
 * RETURNS:
 * void
 *
 * NOTES:
 * Method is nop in release mode.
 * For debug it is useful to see the raw hex contents of a buffer for checks such as HMAC verification.
 */
void debug_print_buffer(const char *prompt, const unsigned char *buffer, const size_t size) {
#ifndef NDEBUG
    printf(prompt);
    for (size_t i = 0; i < size; ++i) {
        printf("%02x", buffer[i]);
    }
    printf("\n");
#else
    (void)(prompt);
    (void)(buffer);
    (void)(size);
#endif
}

/*
 * FUNCTION: checked_malloc
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
 * void *checked_malloc(const size_t size);
 *
 * PARAMETERS:
 * const size_t - The size to allocate
 *
 * RETURNS:
 * void * - The allocated buffer
 *
 * NOTES:
 * Simple wrapper to check for out of memory.
 */
void *checked_malloc(const size_t size) {
    void *rtn = malloc(size);
    if (rtn == NULL) {
        fatal_error("malloc");
    }
    return rtn;
}

/*
 * FUNCTION: checked_calloc
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
 * void *checked_calloc(const size_t nmemb, const size_t size);
 *
 * PARAMETERS:
 * const size_t nmemb - The number of items to allocate
 * const size_t - The size of each member
 *
 * RETURNS:
 * void * - The allocated buffer
 *
 * NOTES:
 * Simple wrapper to check for out of memory.
 */
void *checked_calloc(const size_t nmemb, const size_t size) {
    void *rtn = calloc(nmemb, size);
    if (rtn == NULL) {
        fatal_error("calloc");
    }
    return rtn;
}

/*
 * FUNCTION: checked_realloc
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
 * void *checked_realloc(void *ptr, const size_t size);
 *
 * PARAMETERS:
 * void *ptr - The old pointer
 * const size_t - The size to allocate
 *
 * RETURNS:
 * void * - The reallocated buffer
 *
 * NOTES:
 * Simple wrapper to check for out of memory.
 */
void *checked_realloc(void *ptr, const size_t size) {
    void *rtn = realloc(ptr, size);
    if (rtn == NULL) {
        fatal_error("realloc");
    }
    return rtn;
}
