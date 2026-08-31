#ifndef FRAMER_H
#define FRAMER_H

#include "buf.h"
#include "proto.h"

#include <stddef.h>

typedef struct {
    int fd;
    Buf in;
} Framer;

typedef enum {
    FRAMER_OK = 1,
    FRAMER_EOF = 2,
    FRAMER_MALFORMED = 3,
    FRAMER_EIO = 4,
} FramerStatus;

typedef struct {
    FramerStatus status;
    const char *reason;
} FramerResult;

/* Borrows fd; caller closes it after framer_destroy. */
void framer_init(Framer *fr, int fd);
void framer_destroy(Framer *fr);

int framer_send(const Framer *fr, const Msg *m);
int framer_send_raw(const Framer *fr, const void *data, size_t len);

/* Blocks until a whole message arrives. `out` is only valid on FRAMER_OK. */
FramerResult framer_recv(Framer *fr, Msg *out);

#endif
