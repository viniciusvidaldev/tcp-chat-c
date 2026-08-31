#include "framer.h"
#include "net.h"
#include "proto.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#define READ_CHUNK 1024

void framer_init(Framer *fr, int fd) {
    fr->fd = fd;
    buf_init(&fr->in);
}

void framer_destroy(Framer *fr) { buf_destroy(&fr->in); }

int framer_send(const Framer *fr, const Msg *m) {
    uint8_t buf[MAX_FRAME];
    EncodeResult e = proto_encode_into(m, buf, sizeof buf);
    if (e.status != ENCODE_OK) return -1;
    return framer_send_raw(fr, buf, e.nwritten);
}

int framer_send_raw(const Framer *fr, const void *data, size_t len) {
    return send_all(fr->fd, data, len);
}

FramerResult framer_recv(Framer *fr, Msg *out) {
    for (;;) {
        DecodeResult d = proto_decode(&fr->in, out);
        if (d.status == DECODE_OK) return (FramerResult){FRAMER_OK, NULL};
        if (d.status == DECODE_MALFORMED) return (FramerResult){FRAMER_MALFORMED, d.reason};

        buf_reserve(&fr->in, READ_CHUNK);
        ssize_t n = recv(fr->fd, buf_spare(&fr->in), buf_spare_len(&fr->in), 0);
        if (n == 0) return (FramerResult){FRAMER_EOF, NULL};
        if (n < 0) return (FramerResult){FRAMER_EIO, "recv failed"};
        buf_commit(&fr->in, (size_t)n);
    }
}
