#ifndef CONN_H
#define CONN_H
#include "framer.h"
#include "packet.h"
#include "proto.h"
#include "sendq.h"
#include <sys/socket.h>

typedef struct {
    Framer fr;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    char nick[MAX_NICK + 1];
    SendQ q;
} Conn;

typedef enum {
    CONN_OK = 1,
    CONN_EOF = 2,
    CONN_MALFORMED = 3,
    CONN_EIO = 4,
} ConnStatus;

typedef struct {
    ConnStatus status;
    const char *reason;
} ConnResult;

/* Takes ownership of fd. Returns NULL and closes fd on failure. */
Conn *conn_new(int fd, const struct sockaddr_storage *addr, socklen_t addrlen);

/* Caller must have joined every thread using this conn first. */
void conn_free(Conn *c);

/* Stops the writer and wakes a blocked reader. Idempotent. */
void conn_shutdown(Conn *c);

/* Blocks until a whole message arrives. `out` is only valid on CONN_OK. */
ConnResult conn_recv(Conn *c, Msg *out);

/* Any thread. Retains p on success; the caller keeps its own reference
   either way. -1 if the queue is full or closed. */
int conn_send(Conn *c, Packet *p);

/* Valid until this thread calls conn_peer again. */
const char *conn_peer(const Conn *c);

#endif
