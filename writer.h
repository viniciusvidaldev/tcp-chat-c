#ifndef WRITER_H
#define WRITER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t *p;
    size_t cap;
    size_t pos;
} Writer;

static inline Writer wr_new(uint8_t *p, size_t cap) {
    return (Writer){.p = p, .cap = cap, .pos = 0};
}

static inline size_t wr_spare(Writer *w) { return w->cap - w->pos; }

static inline int wr_u8(Writer *w, uint8_t v) {
    if (wr_spare(w) < 1) return -1;
    w->p[w->pos++] = v;
    return 0;
}

static inline int wr_u16(Writer *w, uint16_t v) {
    if (wr_spare(w) < 2) return -1;
    w->p[w->pos++] = (uint8_t)v >> 8;
    w->p[w->pos++] = (uint8_t)v;
    return 0;
}

static inline int wr_bytes(Writer *w, const void *src, size_t n) {
    if (wr_spare(w) < n) return -1;
    memcpy(w->p + w->pos, src, n);
    w->pos += n;
    return 0;
}

#endif
