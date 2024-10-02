#ifndef __SOCKETS_H
#define __SOCKETS_H

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>

#include "mip_protocol.h"

#define MAX_CONNECTIONS 10
#define MAX_EPOLL_EVENTS 10

int create_server_ipc_socket(char* socket_application, int type);

int create_client_ipc_socket(char* socket_application, int type);

int create_raw_socket();

int epoll_setup();

int add_epoll_socket(int socket_fd, int epoll_fd);

int remove_epoll_socket(int socket_fd, int epoll_fd);

int accept_ipc_connection(int server_fd, int epoll_fd);

#endif
