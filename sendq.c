#include "sendq.h"
#include "check.h"

void sendq_init(SendQ *q) {
    q->head = 0;
    q->len = 0;
    q->closed = false;
    CHECK(pthread_mutex_init(&q->lock, NULL) == 0);
    CHECK(pthread_cond_init(&q->not_empty, NULL) == 0);
}

void sendq_destroy(SendQ *q) {
    while (q->len > 0) {
        packet_release(q->buf[q->head]);
        q->head = (q->head + 1) % SENDQ_CAP;
        q->len--;
    }
    pthread_cond_destroy(&q->not_empty);
    pthread_mutex_destroy(&q->lock);
}

int sendq_send(SendQ *q, Packet *p) {
    pthread_mutex_lock(&q->lock);
    if (q->closed || q->len == SENDQ_CAP) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    size_t slot = (q->head + q->len) % SENDQ_CAP;
    q->buf[slot] = p;
    q->len++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int sendq_recv(SendQ *q, Packet **out) {
    pthread_mutex_lock(&q->lock);
    while (q->len == 0 && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->len == 0) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out = q->buf[q->head];
    q->head = (q->head + 1) % SENDQ_CAP;
    q->len--;
    pthread_mutex_unlock(&q->lock);
    return 0;
}

void sendq_close(SendQ *q) {
    pthread_mutex_lock(&q->lock);
    q->closed = true;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}
