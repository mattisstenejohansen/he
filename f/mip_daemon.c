#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "linked_list.h"
#include "sockets.h"
#include "interface.h"
#include "routing.h"
#include "ping.h"
#include "mip_protocol.h"
#include "mip_arp.h"
#include "message_queue.h"

int term_signal = 0;

void handle_signal(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        term_signal = 1;
    }
}

int handle_arguments(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: [-h] [-d] <socket_application> <socket_exchange> <socket_forward> <mip_addresses>\n");
        printf(" Runs a MIP daemon which handles communication between hosts according to the MIP protocol.\n");
        printf(" The options are:\n");
        printf("  -h: Displays this information\n");
        printf("  -d: Enables debug outputs while running\n");
        printf("  <socket_application>: Pathname to socket to use for communication with applications\n");
        printf("  <socket_exchange>: Pathname to socket to use for exchange of routing information\n");
        printf("  <soket_forward>: Pathname to socket to use for routing lookups\n");
        printf("  <mip_addresses>: MIP addresses which will be mapped to the interfaces\n");
        return -1;
    }

    if (argc < 5) {
        printf("Usage: [-h] [-d] <socket_application> <socket_exchange> <socket_forward> <mip_addresses>\n");
        return -1;
    }

    if (strcmp(argv[1], "-d") == 0) {
        printf("Debugging is switched on\n");
        return 1;
    }

    return 0;
}

int handle_application_message(int app_connection, int fwd_connection, int raw_connection, struct linked_list* queue, struct linked_list* interfaces, int debug) {
    if (debug) printf("Recieving IPC Message\n");

    struct app_message* msg = recieve_app_message(app_connection);
    if (msg == NULL) {
        perror("Could not recieve any data from application. Did the client time out?\n");
        close(app_connection);
    } else {
        if (debug) printf("Recieved Message: %s going to host: %d\n", msg->message, msg->dest);

        // Handle Sending to itself
        struct linked_list_node* cur_node = interfaces->next;
        while (cur_node != NULL) {
            struct interface* interface = (struct interface*) cur_node->data;
            if (interface->mip_address == msg->dest) {
                if (debug) printf("Sending to self!\n");
                send_app_message(app_connection, msg->dest, msg->message);
                free(msg);
                return 0;
            }
            cur_node = cur_node->next;
        }

        // Get Next Hop
        dvr_send_forwarding_message(fwd_connection, msg->dest);
        queue_message(queue, msg->dest, msg->message);
    	free(msg);
        return 1;
    }

    return 0;
}

void handle_routing_exchange_message(int routing_exc_conn, int raw_connection, struct linked_list* interfaces, int debug) {
    char* message = dvr_recieve_distance_vectors(routing_exc_conn);

    // Broadcast routing frame on all interfaces
    struct linked_list_node* cur_node = interfaces->next;
    while (cur_node != NULL) {
        struct interface* interface = (struct interface*) cur_node->data;
        memcpy(&message[0], &interface->mip_address, sizeof(uint8_t));
        if (debug) printf("Sending Routing Table Data on interface %d\n", interface->mip_address);
        send_routing_frame(raw_connection, &interface->data, interface->mip_address, ROUTING_BROADCAST_MIP, message);
        cur_node = cur_node->next;
    }

    free(message);
}

void handle_routing_forwarding_message(int routing_fwd_conn, int app_connection, int raw_connection, struct linked_list* cache, struct linked_list* interfaces, struct linked_list* queue, uint8_t* endpoint, int* endpoint_flag, int debug) {
    // Recieve next hop
    struct app_message* message = get_message(queue);
    uint8_t next_hop = dvr_recieve_forwarding_message(routing_fwd_conn);
    if (debug) printf("Next Hop for %d is %d\n", message->dest, next_hop);

    // Send via IPC if destination has been reached
    struct linked_list_node* cur_node = interfaces->next;
    while (cur_node != NULL) {
        struct interface* interface = (struct interface*) cur_node->data;
        if (interface->mip_address == next_hop) {
            struct app_message* rmessage = pop_message(queue);
            if (debug) printf("Sending to IPC: %s\n", rmessage->message);
            send_app_message(app_connection, *endpoint, rmessage->message);
            *endpoint_flag = 0;
            free(rmessage);
            return;
        }
        cur_node = cur_node->next;
    }

    // Build App Message
    char buffer[sizeof(uint8_t) + strlen(message->message) + 1];
    memset(buffer, 0, sizeof(uint8_t) + strlen(message->message) + 1);
    memcpy(&buffer[0], &message->dest, sizeof(uint8_t));
    memcpy(&buffer[1], &message->message[0], strlen(message->message));
    
    // Forward Message
    int code = send_frame(raw_connection, cache, interfaces, next_hop, buffer, *endpoint, debug);
    if (code == MIP_CODE_ARP_BROADCAST_SENT) {
        // ARP takes care of things.
    } else if (code == MIP_CODE_FRAME_SENT) {
        struct app_message* sent_message = pop_message(queue);
        free(sent_message);
    } else if (code == MIP_CODE_LINK_DOWN) {
        struct app_message* sent_message = pop_message(queue);
        free(sent_message);
        dvr_notify_link_down(routing_fwd_conn, next_hop);
    }
}

void handle_transport(int fwd_connection, struct mip_frame* frame, struct linked_list* queue, struct linked_list* interfaces, char* message, int debug) {

    // Send Forwarding Request to Routing Daemon
    frame->ttl--;
    if (frame->ttl <= 0) {
        if (debug) printf("TTL is 0! Dropping packet!\n");
        return;
    }

    // Deconstruct Message
    struct app_message* msg = calloc(1, sizeof(struct app_message));
    memcpy(&msg->dest, &message[0], sizeof(uint8_t));
    memcpy(&msg->message, &message[1], strlen(message));

    dvr_send_forwarding_message(fwd_connection, msg->dest);
    queue_message(queue, msg->dest, msg->message);
    free(msg);
}

void handle_routing(int routing_exc_conn, char message[]) {
    dvr_send_distance_vector_buffer(routing_exc_conn, message);
}

void send_mip_addresses(int routing_connection, int mip_addresses[], uint8_t num_mip_addresses) {
    size_t buffer_size = sizeof(uint8_t) + num_mip_addresses * sizeof(uint8_t);
    char* buffer = (char*) calloc(1, buffer_size);
    memcpy(&buffer[0], &num_mip_addresses, sizeof(uint8_t));

    int i;
    for (i = 0; i < num_mip_addresses; i++) {
        memcpy(&buffer[i + 1], &mip_addresses[i], sizeof(uint8_t));
    }

    if (send(routing_connection, buffer, buffer_size, 0) < 0) {
        perror("send");
    }

    free(buffer);
}

void handle_arp_broadcast(int raw_connection, struct ethernet_frame* eth_frame, struct mip_frame* frame, struct linked_list* cache, struct linked_list* interfaces, int debug) {
    if (debug) printf("ARP BROADCAST: Dest - %d\n", frame->dest);
    struct interface* interface = get_interface(interfaces, frame->dest);
    if (interface != NULL) {
        if (debug) printf("Sending ARP Response!\n");
        send_arp_response_frame(raw_connection, &interface->data, eth_frame->src, interface->mip_address, frame->src);
        if (debug) printf("Sent ARP Response!\n");
        add_mip_arp_entry(cache, frame->src, eth_frame->src, interface->mip_address);
    }
}

void handle_arp_response(int raw_connection, int fwd_conn, int* endpoint_flag, uint8_t* endpoint, struct ethernet_frame* eth_frame, struct mip_frame* frame, struct linked_list* queue, struct linked_list* cache, struct linked_list* interfaces, int debug) {
    if (debug) printf("Recieved ARP Response!\n");

    struct interface* interface = get_interface(interfaces, frame->dest);
    if (interface != NULL) {
        add_mip_arp_entry(cache, frame->src, eth_frame->src, interface->mip_address);
        
        struct app_message* message = pop_message(queue);
        char msg[sizeof(uint8_t) + strlen(message->message) + 1];
	    memset(&msg, 0, sizeof(uint8_t) + strlen(message->message) + 1);
	    memcpy(&msg[0], &message->dest, sizeof(uint8_t));
        memcpy(&msg[1], message->message, strlen(message->message));
        if (*endpoint_flag == 1) *endpoint = frame->dest;
        int code = send_mip_frame(raw_connection, &interface->data, eth_frame->dest, *endpoint, frame->src, msg);
        if (code == MIP_CODE_LINK_DOWN) {
            dvr_notify_link_down(fwd_conn, frame->src);
        }
        free(message);
    } else {
        printf ("Could not find interface and send cached message\n");
    }
}

int main(int argc, char* argv[]) {

    // Handle Arguments
    int i, j = 0;
    int debug = handle_arguments(argc, argv);
    if (debug < 0) return -1;
    
    char* socket_application = argv[1 + debug];
    char* socket_exchange = argv[2 + debug];
    char* socket_forward = argv[3 + debug];

    int num_mip_addresses = argc - debug - 4;

    int mip_addresses[num_mip_addresses];
    for (i = 0; i < num_mip_addresses; i++) {
        mip_addresses[i] = atoi(argv[i + 4 + debug]);
    }

    // Domain Socket Connection Data
    int domain_connection = -1;
    int routing_fwd_connection = -1;
    int routing_exc_connection = -1;

    // Used for routing messages to endpoints and back
    uint8_t endpoint_src = 0;
    int endpoint_flag = 0;

    // Set Up Epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        return -1;
    }
    
    // Create Domain Sockets
    int domain_socket_fd = create_server_ipc_socket(socket_application, SOCK_STREAM);
    if (domain_socket_fd == -1) return -1;
    if (debug) printf("Domain Socket FD: %d\n", domain_socket_fd);

    int routing_exc_socket = create_server_ipc_socket(socket_exchange, SOCK_SEQPACKET);
    if (routing_exc_socket == -1) return -1;
    if (debug) printf("Routing Exchange Socket FD: %d\n", routing_exc_socket);

    int routing_fwd_socket = create_server_ipc_socket(socket_forward, SOCK_SEQPACKET);
    if (routing_fwd_socket == -1) return -1;
    if (debug) printf("Routing Forwarding Socket FD: %d\n", routing_fwd_socket);

    // Create Raw Sockets
    int raw_send_fd = create_raw_socket();
    if (raw_send_fd == -1) return -1;

    int raw_recv_fd = create_raw_socket();
    if (raw_send_fd == -1) return -1;

    // Enumerate Interfaces and map them to a MIP address
    struct linked_list* interfaces = create_linked_list(sizeof(struct interface));
    num_mip_addresses = enumerate_interfaces(interfaces, mip_addresses, num_mip_addresses, debug);

    // Add Sockets to epoll
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int event_count;
    add_epoll_socket(domain_socket_fd, epoll_fd);
    add_epoll_socket(routing_exc_socket, epoll_fd);
    add_epoll_socket(routing_fwd_socket, epoll_fd);
    add_epoll_socket(raw_send_fd, epoll_fd);
    add_epoll_socket(raw_recv_fd, epoll_fd);

    // Signal Hooks
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // MIP-ARP Cache
    struct linked_list* cache = create_linked_list(sizeof(struct mip_arp_entry));

    // Message Queue
    struct linked_list* queue = create_linked_list(sizeof(struct app_message));

    // Multiplex Sockets w/ epoll
    int running = 1;
    while (running) {
        if (term_signal) running = 0;
        event_count = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);

        for (i = 0; i < event_count; i++) {
            // Handle Incoming Domain Socket Connections
            if (events[i].data.fd == domain_socket_fd) {
                domain_connection = accept_ipc_connection(domain_socket_fd, epoll_fd);
                if (domain_connection == -1) perror("accept_ipc_connection");
                if (debug) printf("Application Connection FD: %d\n", domain_connection);
            } else if (events[i].data.fd == routing_fwd_socket) {
                routing_fwd_connection = accept_ipc_connection(routing_fwd_socket, epoll_fd);
                if (routing_fwd_connection == -1) perror("accept_ipc_connection");
                if (debug) printf("Routing Forward Socket Connection FD: %d\n", routing_fwd_connection);
            } else if (events[i].data.fd == routing_exc_socket) {
                routing_exc_connection = accept_ipc_connection(routing_exc_socket, epoll_fd);
                if (routing_exc_connection == -1) perror("accept_ipc_connection");
                if (debug) printf("Routing Exchange Socket Connection FD: %d\n", routing_exc_connection);
                send_mip_addresses(routing_exc_connection, mip_addresses, num_mip_addresses);
            }

            // Handle Closing of Domain Socket Connections
	        else if (events[i].events & EPOLLHUP) {
                if (events[i].data.fd == domain_connection) {
                    close(domain_connection);
		            domain_connection = -1;
		        } else if (events[i].data.fd == routing_fwd_connection) {
                    close(routing_fwd_connection);
                    if (debug) printf("Closing Routing Forward Connection\n");
                    routing_fwd_connection = -1;
                } else if (events[i].data.fd == routing_exc_connection) {
                    close(routing_exc_connection);
                    if (debug) printf("Closing Routing Exchange Connection\n");
                    routing_exc_connection = -1;
                }
	        }

            // Data to be read on some socket
            else if (events[i].events & EPOLLIN) {
                // Domain Sockets
                if (events[i].data.fd == domain_connection) {
                    endpoint_flag = handle_application_message(domain_connection, routing_fwd_connection, raw_send_fd, queue, interfaces, debug);
                    printf("Endpoint Flag %d\n", endpoint_flag);
                } else if (events[i].data.fd == routing_fwd_connection) {
                    handle_routing_forwarding_message(routing_fwd_connection, domain_connection, raw_send_fd, cache, interfaces, queue, &endpoint_src, &endpoint_flag, debug);
                } else if (events[i].data.fd == routing_exc_connection) {
                    handle_routing_exchange_message(routing_exc_connection, raw_send_fd, interfaces, debug);

                // Raw Socket
                } else if (events[i].data.fd == raw_recv_fd) {
                    if (debug) printf("Recieving MIP Frame on Socket: %d\n", events[i].data.fd);
                    char buffer[MAX_MIP_FRAME_SIZE + sizeof(struct ethernet_frame)];
                    ssize_t bytes_recv = recv(events[i].data.fd, buffer, MAX_MIP_FRAME_SIZE + sizeof(struct ethernet_frame), 0);
                    if (bytes_recv <= 0) {
                        perror("recv");
                    }

                    if (debug) printf("Recieved MIP Frame of %lu bytes!\n", bytes_recv);
                    struct ethernet_frame* eth_frame = (struct ethernet_frame*) buffer;
                    struct mip_frame* frame = (struct mip_frame*) eth_frame->content;
                    size_t payload_len = frame->payload_len * 4;
                    if (debug) printf("Src: %d\nDest: %d\n", frame->src, frame->dest);
		            
                    // What kind of packet is this? - Inspect TRA bits
                    if (frame->tra == TRA_TRANSPORT_BIT) {
                        if (endpoint_flag == 0) endpoint_src = frame->src;
                        handle_transport(routing_fwd_connection, frame, queue, interfaces, frame->content, debug);
                    } else if (frame->tra == TRA_ROUTING_BIT) {
            			uint8_t len;
		            	memcpy(&len, &frame->content[1], sizeof(uint8_t));
			            char fmsg[payload_len];
			            memcpy(fmsg, frame->content, sizeof(uint8_t) * 2 + len * 3 * sizeof(uint8_t));
                        handle_routing(routing_exc_connection, fmsg);
                    } else if (frame->tra == TRA_ARP_BROADCAST_BIT) {
                        handle_arp_broadcast(raw_send_fd, eth_frame, frame, cache, interfaces, debug);
                    } else if (frame->tra == TRA_ARP_RESPONSE_BIT) {
                        handle_arp_response(raw_send_fd, routing_fwd_connection, &endpoint_flag, &endpoint_src, eth_frame, frame, queue, cache, interfaces, debug); 
                    }
                }
            }
        }
    }

    free_linked_list(interfaces);
    free_linked_list(queue);
    free_linked_list(cache);
    close(epoll_fd);
    return 0;
}
