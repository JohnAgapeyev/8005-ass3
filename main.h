/*
 * HEADER FILE: main.h - Program entry and global calls
 *
 * PROGRAM: 7005-asn4
 *
 * DATE: Dec. 2, 2017
 *
 * FUNCTIONS:
 * char *getUserInput(const char *prompt);
 * void debug_print_buffer(const char *prompt, const unsigned char *buffer, const size_t size);
 * void *checked_malloc(const size_t size);
 * void *checked_calloc(const size_t nmemb, const size_t size);
 * void *checked_realloc(void *ptr, const size_t size);
 *
 * VARIABLES:
 * volatile sig_atomic_t isRunning - Whether the application is running
 * extern int outputFD - The file descriptor to output packet contents to
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#ifndef MAIN_H
#define MAIN_H

#include <signal.h>

volatile sig_atomic_t isRunning;

void debug_print_buffer(const char *prompt, const unsigned char *buffer, const size_t size);

void *checked_malloc(const size_t size);
void *checked_calloc(const size_t nmemb, const size_t size);
void *checked_realloc(void *ptr, const size_t size);

#endif
