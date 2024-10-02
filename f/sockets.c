#include "sockets.h"

int create_server_ipc_socket(char* socket_application, int type) {
    int sock_fd;
    
    sock_fd = socket(AF_UNIX, type, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un sock_addr;
    memset(&sock_addr, 0, sizeof(struct sockaddr_un));
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, socket_application, sizeof(sock_addr.sun_path) - 1);
    unlink(sock_addr.sun_path);

    if (bind(sock_fd, (const struct sockaddr*) &sock_addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        return -1;
    }

    if (listen(sock_fd, MAX_CONNECTIONS) == -1) {
        perror("listen");
        return -1;
    }

    return sock_fd;
}

int create_client_ipc_socket(char* socket_application, int type) {
    int sock_fd;

    sock_fd = socket(AF_UNIX, type, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un sock_addr;
    memset(&sock_addr, 0, sizeof(struct sockaddr_un));
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, socket_application, sizeof(sock_addr.sun_path) - 1);

    if (connect(sock_fd, (const struct sockaddr*) &sock_addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        return -1;
    }

    return sock_fd;
}

int create_raw_socket() {
    int sock_fd;

    sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    return sock_fd;
}

int add_epoll_socket(int socket_fd, int epoll_fd) {
    struct epoll_event event;
    memset(&event, 0, sizeof(struct epoll_event));
    event.data.fd = socket_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

int remove_epoll_socket(int socket_fd, int epoll_fd) {
    struct epoll_event event;
    memset(&event, 0, sizeof(struct epoll_event));
    event.data.fd = socket_fd;
    event.events = EPOLLIN | EPOLLHUP; // Not sure if EPOLLHUP is needed.
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_fd, &event) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

int accept_ipc_connection(int server_fd, int epoll_fd) {
    struct sockaddr_un remote;
    int t = sizeof(remote);

    int sock_fd = accept(server_fd, (struct sockaddr*) &remote, &t);
    if (sock_fd == -1) {
        perror("accept");
        return -1;
    }

    if (add_epoll_socket(sock_fd, epoll_fd) == -1) {
        perror("add_epoll_socket");
        return -1;
    }

    return sock_fd;
}
