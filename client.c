#include "framer.h"
#include "net.h"
#include "proto.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static void print_msg(const Msg *m) {
    switch (m->tag) {
    case MSG_TEXT: printf("%s> %s\n", m->as.say.nick, m->as.say.text); break;
    case MSG_JOIN: printf("* %s joined\n", m->as.nick); break;
    case MSG_PART: printf("* %s left\n", m->as.nick); break;
    case MSG_ERR: printf("! %s\n", m->as.text); break;
    case MSG_OK: printf("* %s\n", m->as.text); break;
    default: break;
    }
}

static void *reader(void *arg) {
    Framer *fr = arg;
    for (;;) {
        Msg m;
        FramerResult r = framer_recv(fr, &m);
        if (r.status != FRAMER_OK) {
            if (r.status != FRAMER_EOF) fprintf(stderr, "client -> %s\n", r.reason);
            break;
        }
        print_msg(&m);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    char *host = argv[1];
    char *port = argv[2];
    int fd = connect_socket(host, port);
    if (fd == -1) return 1;

    Framer fr;
    framer_init(&fr, fd);

    printf("nick (max 32 chars): ");
    // \n on enter and \0 that fgets adds
    char nick[MAX_NICK + 2];
    fflush(stdout);

    if (fgets(nick, sizeof nick, stdin) == NULL) {
        fprintf(stderr, "no nick given\n");
        return 1;
    }

    // finds and replace the \n
    nick[strcspn(nick, "\n")] = '\0';

    Msg hello = msg_nick(nick);
    if (framer_send(&fr, &hello) < 0) {
        fprintf(stderr, "client -> send nick failed\n");
        return 1;
    }

    Msg reply;
    FramerResult r = framer_recv(&fr, &reply);
    if (r.status != FRAMER_OK) {
        fprintf(stderr, "client -> %s\n", r.reason);
        return 1;
    }
    if (reply.tag != MSG_OK) {
        fprintf(stderr, "client -> rejected: %s\n", reply.as.text);
        return 1;
    }
    printf("* %s\n", reply.as.text);

    pthread_t rtid;
    if (pthread_create(&rtid, NULL, reader, &fr) != 0) {
        fprintf(stderr, "client -> pthread_create failed\n");
        return 1;
    }

    char line[MAX_TEXT + 2];
    while (fgets(line, sizeof line, stdin) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;
        Msg say = msg_say(line);
        if (framer_send(&fr, &say) < 0) break;
    }

    shutdown(fd, SHUT_RDWR);
    pthread_join(rtid, NULL);
    framer_destroy(&fr);
    close(fd);

    return 0;
}
