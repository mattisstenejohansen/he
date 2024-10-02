#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>

#include "ping.h"
#include "sockets.h"

#define MAX_EVENTS 3

void print_help() {
    printf("Usage: ping_server [-h] <socket_application>\n");
    printf(" Runs a ping server which communicates with the MIP daemon on <socket_application>\n");
    printf(" The options are:\n");
    printf("  -h: Displays this information\n");
    printf("  <socket_application>: Pathname to socket\n");
}

int main(int argc, char* argv[]) {
    // Handle Arguments
    if (argc != 2) {
        printf("Usage: [-h] <socket_application>\n");
        return -1;
    }

    if (strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }

    // Create Socket
    printf("Connecting to MIP Daemon...\n");
    int server_socket_fd = create_client_ipc_socket(argv[1], SOCK_STREAM);

    // Set up epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        close(server_socket_fd);
        return -1;
    }

    struct epoll_event events[MAX_EVENTS];
    int event_count;
    int i;
    
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = server_socket_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket_fd, &event) == -1) {
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_socket_fd);
        return -1;
    }
    
    // Poll Socket and handle incoming messages in a loop
    for(;;) {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (i = 0; i < event_count; i++) {
            if (events[i].events & EPOLLIN) {
                printf("Incoming IPC Message!\n");

                // Recieve IPC Message
                struct app_message* ping = recieve_app_message(server_socket_fd);
                if (ping == NULL) {
                    printf("Something went wrong while recieving the ping message!\n");
                    free(ping);
                    return -1;
                }

                printf("Ping Message: %s\n", ping->message);

                // Relies on MIP Daemon sending source of sender as destination of application message
                send_app_message(server_socket_fd, ping->dest, "PONG");
                free(ping);
            }
        }
    }

    close(epoll_fd);
    close(server_socket_fd);    
    return 0;
}
