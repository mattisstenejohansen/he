#ifndef __PING_H
#define __PING_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1500

// Application Message
struct app_message {
    uint8_t dest;
    char message[BUFFER_SIZE];
};

void send_app_message(int socket_fd, uint8_t destination_host, char* message);

struct app_message* recieve_app_message(int socket_fd);

#endif
