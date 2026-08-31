#include "proto.h"
#include "buf.h"
#include "cursor.h"
#include "writer.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

Msg msg_nick(const char *nick) {
    Msg m = {.tag = MSG_NICK};
    snprintf(m.as.nick, sizeof m.as.nick, "%s", nick);
    return m;
}

Msg msg_say(const char *text) {
    Msg m = {.tag = MSG_SAY};
    snprintf(m.as.text, sizeof m.as.text, "%s", text);
    return m;
}

Msg msg_text(const char *nick, const char *text) {
    Msg m = {.tag = MSG_TEXT};
    snprintf(m.as.say.nick, sizeof m.as.say.nick, "%s", nick);
    snprintf(m.as.say.text, sizeof m.as.say.text, "%s", text);
    return m;
}

Msg msg_join(const char *nick) {
    Msg m = {.tag = MSG_JOIN};
    snprintf(m.as.nick, sizeof m.as.nick, "%s", nick);
    return m;
}

Msg msg_part(const char *nick) {
    Msg m = {.tag = MSG_PART};
    snprintf(m.as.nick, sizeof m.as.nick, "%s", nick);
    return m;
}

Msg msg_ok(const char *text) {
    Msg m = {.tag = MSG_OK};
    snprintf(m.as.text, sizeof m.as.text, "%s", text);
    return m;
}

Msg msg_err(const char *text) {
    Msg m = {.tag = MSG_ERR};
    snprintf(m.as.text, sizeof m.as.text, "%s", text);
    return m;
}

const char *msg_kind_name(MsgKind k) {
    switch (k) {
    case MSG_NICK: return "NICK";
    case MSG_SAY: return "SAY";
    case MSG_TEXT: return "TEXT";
    case MSG_JOIN: return "JOIN";
    case MSG_PART: return "PART";
    case MSG_OK: return "OK";
    case MSG_ERR: return "ERR";
    }
    return "?";
}

/* Reads n bytes as a NUL terminated string. NULL on success, else the reason. */
static const char *cur_field(Cursor *c, char *dst, size_t dst_cap, size_t n) {
    if (n == 0) return "empty field";
    if (cur_str(c, dst, dst_cap, n) < 0) return "field too long";
    if (memchr(dst, '\0', n) != NULL) return "embedded NUL in field";
    return NULL;
}

static DecodeResult body_decode(Cursor *c, Msg *out) {
    uint8_t tag;
    if (cur_u8(c, &tag) < 0) return (DecodeResult){DECODE_MALFORMED, "empty frame, no tag"};

    switch (tag) {
    case MSG_NICK:
    case MSG_JOIN:
    case MSG_PART: {
        /* all the remaining bytes of this frame make the nick */
        size_t n = cur_remaining(c);
        const char *err = cur_field(c, out->as.nick, sizeof out->as.nick, n);
        if (err != NULL) return (DecodeResult){DECODE_MALFORMED, err};

        out->tag = tag;
        return (DecodeResult){DECODE_OK, NULL};
    }

    case MSG_SAY:
    case MSG_OK:
    case MSG_ERR: {
        /* all the remaining bytes of this frame make the text */
        size_t n = cur_remaining(c);
        const char *err = cur_field(c, out->as.text, sizeof out->as.text, n);
        if (err != NULL) return (DecodeResult){DECODE_MALFORMED, err};

        out->tag = tag;
        return (DecodeResult){DECODE_OK, NULL};
    }

    case MSG_TEXT: {
        // get nick: length-prefixed
        uint8_t nick_len;
        if (cur_u8(c, &nick_len) < 0)
            return (DecodeResult){DECODE_MALFORMED, "missing nick length prefix"};

        const char *err = cur_field(c, out->as.say.nick, sizeof out->as.say.nick, nick_len);
        if (err != NULL) return (DecodeResult){DECODE_MALFORMED, err};

        // get text: all remaining bytes are text
        size_t text_len = cur_remaining(c);
        err = cur_field(c, out->as.say.text, sizeof out->as.say.text, text_len);
        if (err != NULL) return (DecodeResult){DECODE_MALFORMED, err};

        out->tag = MSG_TEXT;
        return (DecodeResult){DECODE_OK, NULL};
    }

    default: return (DecodeResult){DECODE_MALFORMED, "unknown tag"};
    }
}

DecodeResult proto_decode(Buf *b, Msg *out) {
    Cursor c = buf_cursor(b);

    uint16_t flen;
    if (cur_u16(&c, &flen) < 0) {
        return (DecodeResult){DECODE_NEED_MORE, "need two bytes for length prefix"};
    }

    if (flen == 0) {
        return (DecodeResult){DECODE_MALFORMED, "length prefix can't be zero"};
    }

    if (flen > MAX_FRAME - LEN_PREFIX) {
        return (DecodeResult){DECODE_MALFORMED, "frame too large"};
    }

    /* Bound the parse to this frame: c still spans later frames in the
           buffer, so a bad length inside the body could read into them. */
    Cursor body;
    if (cur_sub(&c, flen, &body) < 0) {
        /* Hint the buffer to size up for the rest of this frame so the next
                 recv can land it in one shot. Not required for correctness. */
        buf_reserve(b, flen - c.len);
        return (DecodeResult){DECODE_NEED_MORE, "whole frame still hasn't arrived"};
    }

    DecodeResult r = body_decode(&body, out);
    if (r.status != DECODE_OK) return r;
    if (cur_remaining(&body) != 0) {
        return (DecodeResult){DECODE_MALFORMED, "trailing bytes in frame"};
    }

    /* c.pos is prefix + body, and every earlier return
         left the buffer untouched. */
    buf_advance(b, c.pos);
    return (DecodeResult){DECODE_OK, NULL};
};

EncodeResult proto_encode_into(const Msg *m, void *dst, size_t cap) {
    Writer wr = wr_new(dst, cap);

    int rc = 0;
    rc |= wr_u16(&wr, 0);
    rc |= wr_u8(&wr, (uint8_t)m->tag);

    switch (m->tag) {
    case MSG_NICK:
    case MSG_JOIN:
    case MSG_PART: {
        const char *nick = m->as.nick;
        size_t n = strlen(nick);
        if (n == 0) return (EncodeResult){ENCODE_INVALID, "empty nick", 0};
        rc |= wr_bytes(&wr, nick, n);
        break;
    }
    case MSG_SAY:
    case MSG_OK:
    case MSG_ERR: {
        const char *text = m->as.text;
        size_t n = strlen(text);
        if (n == 0) return (EncodeResult){ENCODE_INVALID, "empty text", 0};
        rc |= wr_bytes(&wr, text, n);
        break;
    }
    case MSG_TEXT: {
        const char *nick = m->as.say.nick;
        const char *text = m->as.say.text;
        size_t nl = strlen(nick);
        size_t tl = strlen(text);
        if (nl == 0 || tl == 0) return (EncodeResult){ENCODE_INVALID, "empty nick or text", 0};
        rc |= wr_u8(&wr, (uint8_t)nl);
        rc |= wr_bytes(&wr, nick, nl);
        rc |= wr_bytes(&wr, text, tl);
        break;
    }
    default: return (EncodeResult){ENCODE_INVALID, "unknown tag", 0};
    }

    if (rc < 0) return (EncodeResult){ENCODE_TOO_SMALL, "buffer too small", 0};

    uint16_t flen = (uint16_t)(wr.pos - LEN_PREFIX);
    uint8_t *buf = dst;
    buf[0] = (uint8_t)(flen >> 8);
    buf[1] = (uint8_t)flen;
    return (EncodeResult){ENCODE_OK, NULL, wr.pos};
}
