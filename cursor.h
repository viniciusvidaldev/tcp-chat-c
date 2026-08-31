#ifndef CURSOR_H
#define CURSOR_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const uint8_t *buf;
    size_t pos;
    size_t len;
} Cursor;

static inline Cursor cur_new(const uint8_t *p, size_t len) {
    return (Cursor){
        .buf = p,
        .pos = 0,
        .len = len,
    };
}

static inline size_t cur_remaining(const Cursor *c) { return c->len - c->pos; }

static inline int cur_u8(Cursor *c, uint8_t *out) {
    if (cur_remaining(c) < 1) return -1;
    *out = c->buf[c->pos++];
    return 0;
}

static inline int cur_u16(Cursor *c, uint16_t *out) {
    if (cur_remaining(c) < 2) return -1;
    *out = (c->buf[c->pos] << 8 | c->buf[c->pos + 1]);
    c->pos += 2;
    return 0;
}

// Pointer to the next n bytes, advancing past them. NULL if fewer remain.
static inline const uint8_t *cur_take(Cursor *c, size_t n) {
    if (cur_remaining(c) < n) return NULL;
    const uint8_t *at = c->buf + c->pos;
    c->pos += n;
    return at;
}

static inline int cur_copy(Cursor *c, void *dst, size_t n) {
    const uint8_t *at = cur_take(c, n);
    if (!at) return -1;
    memcpy(dst, at, n);
    return 0;
}

/* NUL terminates, so n must leave room for it within cap. */
static inline int cur_str(Cursor *c, char *dst, size_t cap, size_t n) {
    if (n >= cap) return -1;
    if (cur_copy(c, dst, n) < 0) return -1;
    dst[n] = '\0';
    return 0;
}

/* A cursor over the next n bytes, advancing this one past them. */
static inline int cur_sub(Cursor *c, size_t n, Cursor *out) {
    const uint8_t *at = cur_take(c, n);
    if (!at) return -1;
    *out = cur_new(at, n);
    return 0;
}

#endif
