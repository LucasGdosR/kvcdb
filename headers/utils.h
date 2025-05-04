#pragma once

#include <unistd.h>

#ifdef DEBUG
#define assert(assertion, msg) \
    do { \
        if (!(assertion)) { \
            write(1, msg, sizeof(msg) -1); \
            exit(-1); \
        } \
    } while (0)
#else
#define assert(assertion) //
#endif

#define align(i, alignment) (((i) + (alignment - 1)) & ~(alignment - 1))