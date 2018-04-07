/*
 * HEADER FILE: macro.h - Useful macros
 *
 * PROGRAM: 7005-asn4
 *
 * DATE: Dec. 2, 2017
 *
 * DESIGNER: John Agapeyev
 *
 * PROGRAMMER: John Agapeyev
 */
#ifndef MACRO_H
#define MACRO_H

#include <stddef.h>

#define likely(cond) __builtin_expect(!!(cond), 1)

#define unlikely(cond) __builtin_expect(!!(cond), 0)

#define fatal_error(mesg) \
    do {\
        perror(mesg);\
        fprintf(stderr, "%s, line %d in function %s\n", __FILE__, __LINE__, __func__); \
        exit(EXIT_FAILURE);\
        __builtin_unreachable(); \
    } while(0)

#define container_entry(ptr, type, member)\
    ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

#ifndef NDEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define debug_print(...) \
    do { \
        if (DEBUG) {\
            fprintf(stderr, __VA_ARGS__); \
        } \
    } while(0)

#endif
