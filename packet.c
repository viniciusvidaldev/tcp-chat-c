#include "packet.h"
#include "framer.h"
#include "proto.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

Packet *packet_from_msg(const Msg *m) {
    uint8_t buf[MAX_FRAME];
    EncodeResult e = proto_encode_into(m, buf, sizeof buf);
    if (e.status != ENCODE_OK) return NULL;

    Packet *p = malloc(sizeof *p + e.nwritten);
    if (p == NULL) return NULL;
    atomic_init(&p->refcount, 1);
    p->len = e.nwritten;
    memcpy(p->bytes, buf, e.nwritten);
    return p;
}

int packet_send(const Framer *fr, const Packet *p) { return framer_send_raw(fr, p->bytes, p->len); }

void packet_retain(Packet *p) { atomic_fetch_add_explicit(&p->refcount, 1, memory_order_relaxed); }

void packet_release(Packet *p) {
    if (atomic_fetch_sub_explicit(&p->refcount, 1, memory_order_acq_rel) == 1) {
        free(p);
    }
}
