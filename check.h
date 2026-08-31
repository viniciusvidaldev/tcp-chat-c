#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond);               \
            abort();                                                                               \
        }                                                                                          \
    } while (0)

#ifdef NDEBUG
#define DCHECK(cond) ((void)0)
#else
#define DCHECK(cond) CHECK(cond)
#endif

#endif
