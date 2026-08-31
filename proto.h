#ifndef PROTO_H
#define PROTO_H

#include "buf.h"

#include <stddef.h>
#include <stdint.h>

#define LEN_PREFIX 2

// Biggest frame is MSG_TEXT: [len prefix, tag, nick_len, nick, text].
// Used for capping the len prefix
#define MAX_FRAME (LEN_PREFIX + 1 + 1 + MAX_NICK + MAX_TEXT)

#define MAX_NICK 32
#define MAX_TEXT 512

typedef enum {
    MSG_NICK = 1, /* client to server: requested nick        */
    MSG_SAY = 2,  /* client to server: text only             */
    MSG_TEXT = 3, /* server to client: nick + text           */
    MSG_JOIN = 4, /* server to client: nick joined           */
    MSG_PART = 5, /* server to client: nick left             */
    MSG_OK = 6,   /* server to client: text                  */
    MSG_ERR = 7,  /* server to client: text                  */
} MsgKind;

typedef struct {
    char nick[MAX_NICK + 1];
    char text[MAX_TEXT + 1];
} MsgSay;

typedef union {
    char nick[MAX_NICK + 1]; /* NICK, JOIN, PART */
    char text[MAX_TEXT + 1]; /* SAY, OK, ERR     */
    MsgSay say;              /* TEXT             */
} MsgBody;

typedef struct {
    MsgKind tag;
    MsgBody as;
} Msg;

const char *msg_kind_name(MsgKind k);

Msg msg_nick(const char *nick);
Msg msg_say(const char *text);
Msg msg_text(const char *nick, const char *text);
Msg msg_join(const char *nick);
Msg msg_part(const char *nick);
Msg msg_ok(const char *text);
Msg msg_err(const char *text);

typedef enum {
    DECODE_OK = 1,
    DECODE_NEED_MORE = 0,
    DECODE_MALFORMED = -1,
} DecodeStatus;

typedef struct {
    DecodeStatus status;
    const char *reason;
} DecodeResult;

DecodeResult proto_decode(Buf *in, Msg *out);

typedef enum {
    ENCODE_OK = 1,
    ENCODE_TOO_SMALL = -1,
    ENCODE_INVALID = -2,
} EncodeStatus;

typedef struct {
    EncodeStatus status;
    const char *reason;
    size_t nwritten;
} EncodeResult;

EncodeResult proto_encode_into(const Msg *m, void *dst, size_t cap);

#endif
