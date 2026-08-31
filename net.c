#include "net.h"

#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LISTEN_BACKLOG 128

int listen_socket(const char *port) {
    struct addrinfo *servinfo, hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = getaddrinfo(NULL, port, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "server -> getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    int listen_fd = -1;
    int last_errno = 0;
    const char *last_op = NULL;
    for (struct addrinfo *cur = servinfo; cur != NULL; cur = cur->ai_next) {
        listen_fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (listen_fd == -1) {
            last_errno = errno;
            last_op = "socket";
            continue;
        }

        int yes = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            last_errno = errno;
            last_op = "setsockopt";
            close(listen_fd);
            listen_fd = -1;
            continue;
        }

        if (bind(listen_fd, cur->ai_addr, cur->ai_addrlen) == -1) {
            last_errno = errno;
            last_op = "bind";
            close(listen_fd);
            listen_fd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);
    if (listen_fd == -1) {
        fprintf(stderr, "server -> %s: %s\n", last_op, strerror(last_errno));
        return -1;
    }

    if (listen(listen_fd, LISTEN_BACKLOG) == -1) {
        fprintf(stderr, "server -> listen: %s\n", strerror(errno));
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int connect_socket(const char *host, const char *port) {
    struct addrinfo *servinfo, hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(host, port, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "client -> getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    int fd = -1;
    int last_errno = 0;
    const char *last_op = NULL;
    for (struct addrinfo *cur = servinfo; cur != NULL; cur = cur->ai_next) {
        fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd == -1) {
            last_errno = errno;
            last_op = "socket";
            continue;
        }

        if (connect(fd, cur->ai_addr, cur->ai_addrlen) == -1) {
            last_errno = errno;
            last_op = "connect";
            close(fd);
            fd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);
    if (fd == -1) {
        fprintf(stderr, "client -> %s: %s\n", last_op, strerror(last_errno));
        return -1;
    }

    return fd;
}

const char *addr_str(const struct sockaddr_storage *addr, socklen_t len) {
    static _Thread_local char buf[INET6_ADDRSTRLEN + 8];
    char host[INET6_ADDRSTRLEN];
    char serv[8];

    if (getnameinfo((const struct sockaddr *)addr, len, host, sizeof host, serv, sizeof serv,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
        return "unknown";

    snprintf(buf, sizeof buf, "%s:%s", host, serv);
    return buf;
}

int send_all(int fd, const void *data, size_t len) {
    const uint8_t *p = data;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

ReadStatus read_exact(int fd, void *out, size_t len) {
    uint8_t *p = out;
    size_t got = 0;

    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n > 0) {
            got += n;
            continue;
        }
        if (n == 0) return got == 0 ? READ_EOF : READ_ERR; // EOF mid frame is error
        if (errno == EINTR) continue;
        return READ_ERR;
    }
    return READ_OK;
}
