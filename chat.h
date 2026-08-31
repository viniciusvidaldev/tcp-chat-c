#ifndef CHAT_H
#define CHAT_H

#include <sys/socket.h>

int chat_serve(int fd, const struct sockaddr_storage *addr, socklen_t addrlen);

#endif
