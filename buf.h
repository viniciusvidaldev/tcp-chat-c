#ifndef BUF_H
#define BUF_H

#include "check.h"
#include "cursor.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BUF_MIN_CAP 4096

typedef struct {
    uint8_t *p;
    size_t cap;
    size_t filled;
    size_t consumed;
} Buf;

static inline void buf_init(Buf *b) {
    b->p = NULL;
    b->cap = 0;
    b->consumed = 0;
    b->filled = 0;
};

static inline void buf_destroy(Buf *b) {
    free(b->p);
    buf_init(b);
}

// Chunk is the unread region.
static inline uint8_t *buf_chunk(const Buf *b) { return b->p + b->consumed; }

// Length of the non filled space
static inline size_t buf_spare_len(Buf *b) { return b->cap - b->filled; };

// Amount of unread valid bytes.
static inline size_t buf_len(const Buf *b) { return b->filled - b->consumed; }

// Mark filled bytes as consumed, can be discarded.
static inline void buf_advance(Buf *b, size_t n) {
    CHECK(n <= buf_len(b));
    b->consumed += n;
    if (b->consumed == b->filled) {
        b->consumed = 0;
        b->filled = 0;
    }
}

// Reserve enough space for future use.
static inline void buf_reserve(Buf *b, size_t n) {
    // tail has enough space
    if (buf_spare_len(b) >= n) {
        return;
    }

    size_t live = buf_len(b);

    // compaction is enough to make enough space
    if (b->consumed > 0 && b->cap - live >= n) {
        memmove(b->p, b->p + b->consumed, live);
        b->consumed = 0;
        b->filled = live;
        return;
    }

    CHECK(n <= SIZE_MAX - live);
    size_t need = live + n;
    size_t cap = BUF_MIN_CAP;
    while (cap < need) {
        CHECK(cap <= SIZE_MAX / 2);
        cap *= 2;
    }

    uint8_t *grown = malloc(cap);
    CHECK(grown != NULL);

    if (live > 0) {
        memcpy(grown, b->p + b->consumed, live);
    }
    free(b->p);
    b->p = grown;
    b->filled = live;
    b->consumed = 0;
    b->cap = cap;
}

static inline uint8_t *buf_spare(Buf *b) { return b->p + b->filled; };

static inline void buf_commit(Buf *b, size_t n) {
    CHECK(n <= buf_spare_len(b));
    b->filled += n;
}

static inline Cursor buf_cursor(const Buf *b) { return cur_new(buf_chunk(b), buf_len(b)); }

#endif
