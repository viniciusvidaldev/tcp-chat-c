#include "chat.h"
#include "net.h"

#include <stdio.h>
#include <sys/socket.h>

#define PORT "8008"

int main(void) {
    int listen_fd = listen_socket(PORT);
    if (listen_fd == -1) return 1;

    printf("server: listening on port %s...\n", PORT);

    for (;;) {
        struct sockaddr_storage their_addr;
        socklen_t addrlen = sizeof their_addr;
        int conn_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &addrlen);
        if (conn_fd == -1) {
            perror("server: accept");
            continue;
        }

        chat_serve(conn_fd, &their_addr, addrlen);
    }

    return 0;
}
