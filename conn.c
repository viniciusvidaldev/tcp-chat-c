#include "conn.h"
#include "net.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

Conn *conn_new(int fd, const struct sockaddr_storage *addr, socklen_t addrlen) {
    Conn *c = malloc(sizeof *c);
    if (c == NULL) {
        fprintf(stderr, "conn -> malloc: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    framer_init(&c->fr, fd);
    c->addr = *addr;
    c->addrlen = addrlen;
    c->nick[0] = '\0';
    sendq_init(&c->q);
    return c;
}

void conn_free(Conn *c) {
    close(c->fr.fd);
    framer_destroy(&c->fr);
    sendq_destroy(&c->q);
    free(c);
}

void conn_shutdown(Conn *c) {
    sendq_close(&c->q);
    shutdown(c->fr.fd, SHUT_RDWR);
}

ConnResult conn_recv(Conn *c, Msg *out) {
    FramerResult r = framer_recv(&c->fr, out);
    switch (r.status) {
    case FRAMER_OK: return (ConnResult){CONN_OK, NULL};
    case FRAMER_EOF: return (ConnResult){CONN_EOF, NULL};
    case FRAMER_MALFORMED: return (ConnResult){CONN_MALFORMED, r.reason};
    case FRAMER_EIO: return (ConnResult){CONN_EIO, r.reason};
    }
    return (ConnResult){CONN_EIO, "unknown framer status"};
}

int conn_send(Conn *c, Packet *p) {
    packet_retain(p);
    if (sendq_send(&c->q, p) < 0) {
        packet_release(p);
        return -1;
    }
    return 0;
}

const char *conn_peer(const Conn *c) { return addr_str(&c->addr, c->addrlen); }
