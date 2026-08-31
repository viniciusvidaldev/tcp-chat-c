#include "chat.h"
#include "conn.h"
#include "packet.h"
#include "proto.h"
#include "sendq.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_CONNS 256
#define MAX_ATTEMPTS 5

static pthread_mutex_t reg_lock = PTHREAD_MUTEX_INITIALIZER;
static Conn *reg[MAX_CONNS];
static size_t nreg;

static int send_msg(Conn *c, const Msg *m) {
    Packet *p = packet_from_msg(m);
    if (p == NULL) return -1;
    int rv = conn_send(c, p);
    packet_release(p);
    return rv;
}

static void broadcast(const Conn *skip, const Msg *m) {
    Packet *p = packet_from_msg(m);
    if (p == NULL) return;
    pthread_mutex_lock(&reg_lock);
    for (size_t i = 0; i < nreg; i++) {
        if (reg[i] != skip) conn_send(reg[i], p);
    }
    pthread_mutex_unlock(&reg_lock);
    packet_release(p);
}

typedef enum {
    REG_OK,
    REG_FULL,
    REG_TAKEN,
} RegStatus;

static RegStatus reg_register(Conn *c, const char *name) {
    pthread_mutex_lock(&reg_lock);
    if (nreg == MAX_CONNS) {
        pthread_mutex_unlock(&reg_lock);
        return REG_FULL;
    }
    for (size_t i = 0; i < nreg; i++) {
        if (strcmp(reg[i]->nick, name) == 0) {
            pthread_mutex_unlock(&reg_lock);
            return REG_TAKEN;
        }
    }
    snprintf(c->nick, sizeof c->nick, "%s", name);
    reg[nreg++] = c;
    pthread_mutex_unlock(&reg_lock);
    return REG_OK;
}

static void reg_remove(Conn *c) {
    pthread_mutex_lock(&reg_lock);
    for (size_t i = 0; i < nreg; i++) {
        if (c == reg[i]) {
            reg[i] = reg[--nreg];
            break;
        }
    }
    pthread_mutex_unlock(&reg_lock);
}

static bool nick_valid(const char *n) {
    size_t len = strlen(n);
    if (len == 0 || len > MAX_NICK) return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)n[i];
        if (c < 0x21 || c > 0x7e) return false;
    }
    return true;
}

static int do_handshake(Conn *c) {
    for (size_t attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        Msg m;
        ConnResult r = conn_recv(c, &m);
        if (r.status != CONN_OK) {
            if (r.status != CONN_EOF)
                fprintf(stderr, "chat -> handshake from %s: %s\n", conn_peer(c), r.reason);
            return -1;
        }

        if (m.tag != MSG_NICK) {
            Msg err = msg_err("send NICK first");
            send_msg(c, &err);
            continue;
        }

        const char *nick = m.as.nick;
        if (!nick_valid(nick)) {
            Msg err = msg_err("nick must be printable, no spaces");
            send_msg(c, &err);
            continue;
        }

        switch (reg_register(c, nick)) {
        case REG_OK: {
            Msg ok = msg_ok("welcome");
            send_msg(c, &ok);
            Msg join = msg_join(nick);
            broadcast(c, &join);
            return 0;
        }
        case REG_TAKEN: {
            Msg err = msg_err("nick taken");
            send_msg(c, &err);
            break;
        }
        case REG_FULL: {
            Msg err = msg_err("server full");
            send_msg(c, &err);
            return -1;
        }
        }
    }
    return -1;
}

static void chat_loop(Conn *c) {
    for (;;) {
        Msg m;
        ConnResult r = conn_recv(c, &m);
        if (r.status != CONN_OK) {
            if (r.status != CONN_EOF) fprintf(stderr, "chat -> %s: %s\n", c->nick, r.reason);
            return;
        }
        switch (m.tag) {
        case MSG_SAY: {
            Msg out = msg_text(c->nick, m.as.text);
            broadcast(c, &out);
            break;
        }
        case MSG_NICK: {
            Msg err = msg_err("already registered");
            send_msg(c, &err);
            break;
        }
        default: {
            Msg err = msg_err("message kind not allowed from a client");
            send_msg(c, &err);
            return;
        }
        }
    }
}

static void *writer(void *arg) {
    Conn *c = arg;
    Packet *p;
    while (sendq_recv(&c->q, &p) == 0) {
        int rv = packet_send(&c->fr, p);
        packet_release(p);
        if (rv < 0) break;
    }
    shutdown(c->fr.fd, SHUT_RDWR); /* wake the reader if we died first */
    return NULL;
}

static void *reader(void *arg) {
    Conn *c = arg;

    pthread_t wtid;
    int rv = pthread_create(&wtid, NULL, writer, c);
    if (rv != 0) {
        fprintf(stderr, "chat -> pthread_create writer: %s\n", strerror(rv));
        conn_free(c);
        return NULL;
    }

    if (do_handshake(c) == 0) {
        chat_loop(c);
        reg_remove(c);
        Msg part = msg_part(c->nick);
        broadcast(NULL, &part);
    }

    /* wake the writer wherever it is parked, then wait for it */
    conn_shutdown(c);
    pthread_join(wtid, NULL);
    conn_free(c);
    return NULL;
}

int chat_serve(int fd, const struct sockaddr_storage *addr, socklen_t addrlen) {
    Conn *c = conn_new(fd, addr, addrlen);
    if (c == NULL) return -1;

    pthread_t rtid;
    int rv = pthread_create(&rtid, NULL, reader, c);
    if (rv != 0) {
        fprintf(stderr, "chat -> pthread_create reader: %s\n", strerror(rv));
        conn_free(c);
        return -1;
    }
    pthread_detach(rtid);
    return 0;
}
