#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <sys/socket.h>

int listen_socket(const char *port);
int connect_socket(const char *host, const char *port);
const char *addr_str(const struct sockaddr_storage *addr, socklen_t len);

int send_all(int fd, const void *data, size_t len);

typedef enum {
    READ_OK = 1,
    READ_EOF = 0,
    READ_ERR = -1,
} ReadStatus;

ReadStatus read_exact(int fd, void *out, size_t len);

#endif
