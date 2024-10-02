#include "routing.h"

struct dvr_info* dvr_deconstruct_message(char message[]) {
    struct dvr_info* dvr_info = calloc(1, sizeof(struct dvr_info));
    struct linked_list* table = create_linked_list(sizeof(struct routing_table_entry));

    uint8_t src_mip;
    uint8_t length;
    memcpy(&src_mip, &message[0], sizeof(uint8_t));
    memcpy(&length, &message[1], sizeof(uint8_t));

    int buffer_index = 2;
    int i;
    for (i = 0; i < length; i++) {
        uint8_t mip;
        uint8_t cost;
        uint8_t next_hop;
        memcpy(&mip, &message[buffer_index], sizeof(uint8_t));
        buffer_index++;
        memcpy(&cost, &message[buffer_index], sizeof(uint8_t));
        buffer_index++;
        memcpy(&next_hop, &message[buffer_index], sizeof(uint8_t));
        buffer_index++;
        add_routing_entry(table, mip, next_hop, cost);
    }

    dvr_info->src_mip = src_mip;
    dvr_info->routes = table;
    return dvr_info;
}

void dvr_send_distance_vector_buffer(int socket_fd, char message[]) {
    uint8_t length;
    memcpy(&length, &message[1], sizeof(uint8_t));
    size_t buffer_size = sizeof(uint8_t) * 2 + sizeof(uint8_t) * 3 * length;

    if (send(socket_fd, message, buffer_size, 0) < 0) {
        perror("send");
    }
}


void dvr_send_distance_vectors(int socket_fd, uint8_t src_mip, struct linked_list* routing_table) {
    size_t static_size = sizeof(uint8_t) * 2; // Src MIP, Num routes
    size_t vectors_size = sizeof(uint8_t) * 3 * routing_table->num_elements;
    size_t buffer_size = static_size + vectors_size;

    if (buffer_size > MAX_VECTOR_BUFFER) {
        printf("Could not send distance vectors because buffer will be overflowed\n");
        return;
    }

    char* buffer = (char*) calloc(1, buffer_size);
    memcpy(&buffer[0], &src_mip, sizeof(uint8_t));
    memcpy(&buffer[1], &routing_table->num_elements, sizeof(uint8_t));
    int buffer_index = 2;

    struct linked_list_node* cur_node = routing_table->next;
    while (cur_node != NULL) {
        struct routing_table_entry* entry = (struct routing_table_entry*) cur_node->data;
        memcpy(&buffer[buffer_index], &entry->dest, sizeof(uint8_t));
        buffer_index++;
        memcpy(&buffer[buffer_index], &entry->cost, sizeof(uint8_t));
        buffer_index++;
        memcpy(&buffer[buffer_index], &entry->next_hop, sizeof(uint8_t));
        buffer_index++;
        cur_node = cur_node->next;
    }

    if (send(socket_fd, buffer, buffer_size, 0) < 0) {
        perror("send");
    }

    free(buffer);
}

// Recieves a routing table where next hop is ireelevant
char* dvr_recieve_distance_vectors(int socket_fd) {
    char* buffer = calloc(1, MAX_VECTOR_BUFFER);
    ssize_t recv_bytes = recv(socket_fd, buffer, MAX_VECTOR_BUFFER, 0);
    if (recv_bytes < 0) {
        perror("recv");
        return 0;
    
    }
    return buffer;
}

void dvr_send_forwarding_message(int socket_fd, uint8_t dest_mip) {
    size_t buffer_size = sizeof(uint8_t);
    char buffer[buffer_size];
    memset(buffer, dest_mip, buffer_size);

    if (send(socket_fd, buffer, buffer_size, 0) < 0) {
        perror("send");
        return;
    }
}

uint8_t dvr_recieve_forwarding_message(int socket_fd) {
    size_t buffer_size = sizeof(uint8_t);
    char buffer[buffer_size];
    memset(buffer, 0, buffer_size);

    ssize_t recv_bytes = recv(socket_fd, buffer, buffer_size, 0);
    if (recv_bytes < 0) {
        perror("recv");
        return 0;
    }

    uint8_t next_hop = 0;
    memcpy(&next_hop, &buffer[0], sizeof(uint8_t));

    return next_hop;
}

void dvr_notify_link_down(int socket_fd, uint8_t failed_link_mip) {
    size_t buffer_size = sizeof(uint8_t) * 2;
    char buffer[buffer_size];
    memset(buffer, 0, buffer_size);

    uint8_t code = FAILED_LINK_CODE;
    memcpy(&buffer[0], &code, sizeof(uint8_t));
    memcpy(&buffer[1], &failed_link_mip, sizeof(uint8_t));

    if (send(socket_fd, buffer, buffer_size, 0) < 0) {
        perror("send");
        return;
    }
}
