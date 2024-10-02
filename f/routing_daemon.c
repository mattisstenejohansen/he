#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "linked_list.h"
#include "routing_table.h"
#include "routing.h"
#include "sockets.h"

#define MAX_EPOLL_EVENTS 10
#define ROUTE_EXCHANGE_INTERVAL 15

// Data passed to exchange thread
struct routing_thread_data {
    int socket;
    struct linked_list* routing_table;
};

int term_signal = 0;

void handle_signal(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        printf("SIGTERM Recieved! Terminating Program...\n");
        printf("Exiting the routing exchange thread may take a while\n");
        term_signal = 1;
    }
}

void print_help() {
    printf("Usage: [-h] <socket_exchange> <socket_forward>\n");
    printf(" Runs a routing daemon which handles routing between hosts.\n");
    printf(" The options are:\n");
    printf("  -h: Displays this information\n");
    printf("  <socket_exchange>: Pathname to socket to use for exchange of routing information\n");
    printf("  <socket_forward>: Pathname to socket to use for routing lookups\n");
}

void handle_link_down(uint8_t mip_down, struct linked_list* routing_table) {
    struct routing_table_entry* entry = get_routing_entry(routing_table, mip_down);
    entry->cost = NO_ROUTE;
    printf("Link %d set to %d", entry->dest, entry->cost);
}

void handle_forward_request(int fwd_socket, struct linked_list* routing_table) {
    uint8_t request_mip = dvr_recieve_forwarding_message(fwd_socket);
    if (request_mip == 0) {
        printf("Forwarding Request for destination: %d! Something likely went wrong on recv.\n", request_mip);
        return;
    }

    if (request_mip == FAILED_LINK_CODE) {
        printf("Link down!\n");
        uint8_t mip_down = dvr_recieve_forwarding_message(fwd_socket);
        printf("Handling link...\n");
        handle_link_down(mip_down, routing_table);
        return;
    }

    uint8_t next_hop = get_next_hop(routing_table, request_mip);
    printf("Next hop for %d is %d", request_mip, next_hop);

    dvr_send_forwarding_message(fwd_socket, next_hop);
}

void handle_exchange(int exc_socket, struct linked_list* routing_table, struct linked_list* mip_addresses) {
    char* buffer = dvr_recieve_distance_vectors(exc_socket);
    struct dvr_info* dvr_info = dvr_deconstruct_message(buffer);
    printf("Recieved Distance Vectors!\n");

    uint8_t source_mip = dvr_info->src_mip;

    struct linked_list_node* cur_node = dvr_info->routes->next;
    while (cur_node != NULL) {
        struct routing_table_entry* route = (struct routing_table_entry*) cur_node->data;
        int should_process = 1;

        // Prevent advertising route back to the interface it was learnt;
        struct linked_list_node* cur_mip_node = mip_addresses->next;
        while (cur_mip_node != NULL) {
            uint8_t* cur_mip = (uint8_t*) cur_mip_node->data;
            if (route->next_hop == *cur_mip) {
                should_process = 0;
            }
            cur_mip_node = cur_mip_node->next;
        }

        if (should_process) {
            struct routing_table_entry* entry = get_routing_entry(routing_table, route->dest);
            if (entry == NULL) {
                // Add route if it doesn't exist yet
                add_routing_entry(routing_table, route->dest, source_mip, route->cost + 1);
            } else {
                if (route->cost == NO_ROUTE) {
                    // Link has gone down, need to update entry!
                    entry->cost = NO_ROUTE;
                } else {
                    // Update route if a better path exists
                    if (route->cost + 1 <= entry->cost) {
                        entry->cost = route->cost + 1;
                        entry->next_hop = source_mip;
                    }
                }
            }
        }

        cur_node = cur_node->next;
    }

    print_routing_table(routing_table);

    // Free Stuff
    free(buffer);
    free_linked_list(dvr_info->routes);
    free(dvr_info);
}

void* exchange_thread(void* args) {
    struct routing_thread_data* thread_data = (struct routing_thread_data*) args;
    sleep(1);
    printf("Running Exchange Thread\n");

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Run Exchange Loop
    int running = 1;
    while (running) {
        if (term_signal) running = 0;
        sleep(ROUTE_EXCHANGE_INTERVAL);
        dvr_send_distance_vectors(thread_data->socket, ROUTING_DAEMON_SOURCE, thread_data->routing_table);
    }

    printf("Stopping Exchange Thread\n");
    free(thread_data);
    return 0;
}

int recieve_mip_addresses(int mip_connection, struct linked_list* routing_table, struct linked_list* mip_addresses) {
    // Max MIP addresses = 254, since 255 is reserved for broadcasting
    char buffer[255];
    memset(buffer, 0, 255);

    ssize_t recv_bytes = recv(mip_connection, buffer, 255, 0);
    if (recv_bytes < 0) {
        perror("recv");
        return -1;
    }

    uint8_t num_mips;
    memcpy(&num_mips, &buffer[0], sizeof(uint8_t));

    int i;
    for (i = 0; i < num_mips; i++) {
        uint8_t mip;
        memcpy(&mip, &buffer[i + 1], sizeof(uint8_t));
        add_routing_entry(routing_table, mip, mip, 0);
        printf("Recieved MIP: %d\n", mip);
        add_element(mip_addresses, &mip);
    }

    return 0;
}

int main(int argc, char* argv[]) {
    // Handle Arguments
    if (argc != 3) {
        printf("Usage: [-h] <socket_exchange> <socket_forward>\n");
        return -1;
    }

    if (strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }

    char* socket_exchange = argv[1];
    char* socket_forward = argv[2];

    // Create IPC Sockets for Routing Exchange and Forwarding Lookup
    int exc_socket = create_client_ipc_socket(socket_exchange, SOCK_SEQPACKET);
    int fwd_socket = create_client_ipc_socket(socket_forward, SOCK_SEQPACKET);

    // Set up epoll
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int event_count;
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        close(exc_socket);
        close(fwd_socket);
        return -1;
    }

    // Should be error checked so sockets can be closed
    add_epoll_socket(exc_socket, epoll_fd);
    add_epoll_socket(fwd_socket, epoll_fd);

    // Create Initial Routing Table
    struct linked_list* routing_table = create_linked_list(sizeof(struct routing_table_entry));
    struct linked_list* mip_addresses = create_linked_list(sizeof(uint8_t));
    int ret = recieve_mip_addresses(exc_socket, routing_table, mip_addresses);
    if (ret == -1) {
        perror("Could not recieve MIP addresses of MIP daemon.\n");
        free_linked_list(routing_table);
        return -1;
    }
    printf("Created Initial Routing Table!\n");

    // Send Initial Routing Table to all neighbours
    dvr_send_distance_vectors(exc_socket, ROUTING_DAEMON_SOURCE, routing_table);

    // Set up thread which handles sending routing table every now and then
    pthread_t exchange_thread_id;
    struct routing_thread_data* thread_data = calloc(1, sizeof(struct routing_thread_data));
    thread_data->socket = exc_socket;
    thread_data->routing_table = routing_table;
    if (pthread_create(&exchange_thread_id, NULL, exchange_thread, thread_data)) {
        free(thread_data);
    }

    // Signal Hooks
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Main Loop
    int i;
    int running = 1;
    while (running) {
        if (term_signal) running = 0;
        event_count = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        for (i = 0; i < event_count; i++) {
            if (events[i].events & EPOLLIN) {
                if (events[i].data.fd == exc_socket) {
                    handle_exchange(exc_socket, routing_table, mip_addresses);
                } else if (events[i].data.fd == fwd_socket) {
                    handle_forward_request(fwd_socket, routing_table);
                }
            }
        }
    }

    // Free things from memory
    pthread_join(exchange_thread_id, NULL);
    close(exc_socket);
    close(fwd_socket);
    free_linked_list(mip_addresses);
    free_linked_list(routing_table);
    return 0;
}
