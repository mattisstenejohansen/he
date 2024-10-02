#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

#include "mip_arp.h"
#include "linked_list.h"
#include "interface.h"

// MIP Ethernet Protocol
#define ETH_P_MIP 0x88B5

// MIP Specifications
#define MAX_MIP_ADR 255
#define MAX_MIP_FRAME_SIZE 1500
#define MAX_TTL 15

// Response Codes
#define MIP_CODE_ARP_BROADCAST_SENT 1
#define MIP_CODE_FRAME_SENT 2
#define MIP_CODE_ERROR 3
#define MIP_CODE_LINK_DOWN 4
#define MIP_CODE_OVERFLOW_SIZE 5

// TRA Bits
#define TRA_TRANSPORT_BIT 4
#define TRA_ROUTING_BIT 2
#define TRA_ARP_BROADCAST_BIT 1
#define TRA_ARP_RESPONSE_BIT 0

// Ethernet Frame
struct ethernet_frame {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t protocol;
    uint8_t content[0];
} __attribute__ ((packed));

// MIP Frame
struct mip_frame {
    uint8_t tra:3;
    uint8_t ttl:4;
    uint16_t payload_len:9;
    uint8_t dest;
    uint8_t src;
    uint8_t content[0];
} __attribute__ ((packed));


int send_frame(int raw_connection, struct linked_list* arp_cache, struct linked_list* interfaces, uint8_t dest_mip, char message[], uint8_t endpoint, int debug);

void send_broadcast_frame(int socket_fd, struct sockaddr_ll* interface, uint8_t src, uint8_t dest);

void send_arp_response_frame(int socket_fd, struct sockaddr_ll* interface, unsigned char dest_mac[6], uint8_t src, uint8_t dest);

void send_routing_frame(int socket_fd, struct sockaddr_ll* interface, uint8_t src, uint8_t dest, char* msg);

int send_mip_frame(int socket_fd, struct sockaddr_ll* interface, unsigned char dest_mac[6], uint8_t src, uint8_t dest, char* msg);

#endif
