#ifndef PACKET_H
#define PACKET_H

#include "framer.h"
#include "proto.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    atomic_int refcount;
    size_t len;
    uint8_t bytes[];
} Packet;

Packet *packet_from_msg(const Msg *m);
int packet_send(const Framer *fr, const Packet *p);

void packet_retain(Packet *p);
void packet_release(Packet *p);

#endif
