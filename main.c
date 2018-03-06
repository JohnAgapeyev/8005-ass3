/*
 * SOURCE FILE: main.c - Implementation of functions declared in main.h
 *
 * PROGRAM: 7005-asn4
 *
 * DATE: Dec. 2, 2017
 *
 * FUNCTIONS:
 * static void sighandler(int signo);
 * char *getUserInput(const char *prompt);
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
#include <getopt.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include "main.h"
#include "macro.h"
#include "socket.h"
#include "network.h"

static void sighandler(int signo);

static struct option long_options[] = {
    {"port",    required_argument, 0, 'p'},
    {"help",    no_argument,       0, 'h'},
    {0,         0,                 0, 0}
};

#define print_help() \
    do { \
    printf("usage options:\n"\
            "\t [p]ort <1-65535>        - the port to use, default 1337\n"\
            "\t [h]elp                  - this message\n"\
            );\
    } while(0)

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
 * int main(int argc, char **argv)
 *
 * PARAMETERS:
 * int argc - The number of command arguments
 * char **argv - A list of the command arguments as strings
 *
 * RETURNS:
 * int - The application return code
 *
 * NOTES:
 * Validates command arguments and starts client/server from here
 */
int main(int argc, char **argv) {
    isRunning = ATOMIC_VAR_INIT(1);

    struct sigaction sigHandleList = {.sa_handler=sighandler};
    sigaction(SIGINT,&sigHandleList,0);
    sigaction(SIGHUP,&sigHandleList,0);
    sigaction(SIGQUIT,&sigHandleList,0);
    sigaction(SIGTERM,&sigHandleList,0);

    char *portString = NULL;

    int c;
    for (;;) {
        int option_index = 0;
        if ((c = getopt_long(argc, argv, "p:h", long_options, &option_index)) == -1) {
            break;
        }
        switch (c) {
            case 'p':
                portString = optarg;
                break;
            case 'h':
                //Intentional fallthrough
            case '?':
                //Intentional fallthrough
            default:
                print_help();
                return EXIT_SUCCESS;
        }
    }
    if (portString == NULL) {
        puts("No port set, reverting to port 1337");
        portString = "1337";
    }
    port = strtoul(portString, NULL, 0);
    if (errno == EINVAL || errno == ERANGE) {
        perror("strtoul");
        return EXIT_FAILURE;
    }

    listenSock = createSocket(AF_INET, SOCK_STREAM, 0);
    bindSocket(listenSock, port);
    listen(listenSock, SOMAXCONN);
    startServer();
    close(listenSock);

    return EXIT_SUCCESS;
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
