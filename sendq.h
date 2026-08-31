#ifndef SENDQ_H
#define SENDQ_H
#include "packet.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define SENDQ_CAP 256

typedef struct {
    Packet *buf[SENDQ_CAP];
    size_t head;
    size_t len;
    bool closed;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} SendQ;

void sendq_init(SendQ *q);

void sendq_destroy(SendQ *q);

int sendq_send(SendQ *q, Packet *p);

int sendq_recv(SendQ *q, Packet **out);

void sendq_close(SendQ *q);

#endif
