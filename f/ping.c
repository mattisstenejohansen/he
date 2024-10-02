#include "ping.h"

void send_app_message(int socket_fd, uint8_t destination_host, char* message) {
    char buffer[sizeof(uint8_t) + strlen(message)];
    memset(buffer, 0, sizeof(uint8_t) + strlen(message));
    memcpy(&buffer[0], &destination_host, sizeof(uint8_t));
    memcpy(&buffer[1], message, strlen(message));

    if (send(socket_fd, buffer, sizeof(uint8_t) + strlen(message), 0) < 0) {
        perror("send");
        return;
    }
}

struct app_message* recieve_app_message(int socket_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
        
    ssize_t recv_bytes = recv(socket_fd, buffer, BUFFER_SIZE, 0);
    if (recv_bytes < 0) {
        perror("recv");
        return NULL;
    }

    struct app_message* msg = (struct app_message*) calloc(1, sizeof(struct app_message));
    memcpy(&msg->dest, &buffer[0], sizeof(uint8_t));
    memcpy(&msg->message, &buffer[1], strlen(&buffer[1]));
    return msg;
}
