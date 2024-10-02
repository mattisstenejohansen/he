#ifndef __ROUTING_H
#define __ROUTING_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "linked_list.h"
#include "routing_table.h"

#define MAX_VECTOR_BUFFER 1500
#define ROUTING_BROADCAST_MIP 255
#define FAILED_LINK_CODE 255
#define ROUTING_DAEMON_SOURCE 0

struct dvr_info {
    uint8_t src_mip;
    struct linked_list* routes;
};

struct dvr_info* dvr_deconstruct_message(char message[]);

void dvr_send_distance_vector_buffer(int socket_fd, char message[]);

void dvr_send_distance_vectors(int socket_fd, uint8_t src_mip, struct linked_list* routing_table);

char* dvr_recieve_distance_vectors(int socket_fd);

void dvr_send_forwarding_message(int socket_fd, uint8_t dest_mip);

uint8_t dvr_recieve_forwarding_message(int socket_fd);

void dvr_notify_link_down(int socket_fd, uint8_t failed_link_mip);

#endif
