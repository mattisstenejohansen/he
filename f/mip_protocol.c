#include "mip_protocol.h"

#define ETH_BROADCAST_ADDR {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}

int send_frame(int raw_connection, struct linked_list* arp_cache, struct linked_list* interfaces, uint8_t dest_mip, char message[], uint8_t endpoint,int debug) {
    struct mip_arp_entry* arp_entry = get_mip_arp_entry(arp_cache, dest_mip);

    if (arp_entry == NULL) {
        // ARP Broadcast
        if (debug) printf("No ARP Entry found!\n");
        struct linked_list_node* cur_node = interfaces->next;
        while (cur_node != NULL) {
            struct interface* interface = (struct interface*) cur_node->data;
            send_broadcast_frame(raw_connection, &interface->data, interface->mip_address, dest_mip);
            cur_node = cur_node->next;
        }
        if (debug) printf("ARP Broadcast sent!\n");
        return MIP_CODE_ARP_BROADCAST_SENT;
    } else {
        struct interface* interface = get_interface(interfaces, arp_entry->via_interface);
        if (interface == NULL) {
            printf("Severe Error: Could not look up interface!\n");
            return MIP_CODE_ERROR;
        }

        int code = send_mip_frame(raw_connection, &interface->data, arp_entry->mac_address, endpoint, dest_mip, message);
        return code;
    }
}

void send_broadcast_frame(int socket_fd, struct sockaddr_ll* interface, uint8_t src, uint8_t dest) {
    struct ethernet_frame eth_frame;
    uint8_t broadcast_addr[] = ETH_BROADCAST_ADDR;
    memcpy(eth_frame.dest, broadcast_addr, 6);
    memcpy(eth_frame.src, interface->sll_addr, 6);
    eth_frame.protocol = htons(ETH_P_MIP);

    struct mip_frame m_frame;
    m_frame.tra = TRA_ARP_BROADCAST_BIT;
    m_frame.ttl = MAX_TTL;
    m_frame.payload_len = 0;
    m_frame.dest = dest;
    m_frame.src = src;
    
    struct msghdr* msgh;
    struct iovec iov[2];
    iov[0].iov_base = &eth_frame;
    iov[0].iov_len = sizeof(struct ethernet_frame);
    iov[1].iov_base = &m_frame;
    iov[1].iov_len = sizeof(struct mip_frame);
    msgh = calloc(1, sizeof(struct msghdr));

    struct sockaddr_ll so_name;
    memcpy(&so_name, interface, sizeof(struct sockaddr_ll));
    memcpy(so_name.sll_addr, broadcast_addr, 6); // Warning!
    msgh->msg_name = interface;
    msgh->msg_namelen = sizeof(struct sockaddr_ll);
    msgh->msg_iovlen = 2;
    msgh->msg_iov = iov;

    if (sendmsg(socket_fd, msgh, 0) == -1) {
        perror("sendmsg");
    }
    free(msgh);
}

void send_arp_response_frame(int socket_fd, struct sockaddr_ll* interface, unsigned char dest_mac[6], uint8_t src, uint8_t dest) {
    struct ethernet_frame eth_frame;
    memcpy(eth_frame.dest, dest_mac, 6);
    memcpy(eth_frame.src, interface->sll_addr, 6);
    eth_frame.protocol = htons(ETH_P_MIP);

    struct mip_frame m_frame;
    m_frame.tra = TRA_ARP_RESPONSE_BIT;
    m_frame.ttl = MAX_TTL;
    m_frame.payload_len = 0;
    m_frame.dest = dest;
    m_frame.src = src;

    struct msghdr* msgh;
    struct iovec iov[2];
    iov[0].iov_base = &eth_frame;
    iov[0].iov_len = sizeof(struct ethernet_frame);
    iov[1].iov_base = &m_frame;
    iov[1].iov_len = sizeof(struct mip_frame);
    msgh = calloc(1, sizeof(struct msghdr));

    msgh->msg_name = interface;
    msgh->msg_namelen = sizeof(struct sockaddr_ll);
    msgh->msg_iovlen = 2;
    msgh->msg_iov = iov;

    if (sendmsg(socket_fd, msgh, 0) == -1) {
        perror("sendmsg");
    }
    free(msgh);
}

void send_routing_frame(int socket_fd, struct sockaddr_ll* interface, uint8_t src, uint8_t dest, char* msg) {
    // Calculate Payload Length
    int payload_len;

    uint8_t num_routes;
    memcpy(&num_routes, &msg[1], sizeof(uint8_t));
    int msg_len = sizeof(uint8_t) * 2 + sizeof(uint8_t) * 3 * num_routes;
    printf("Routing Frame Payload Length: %d\n", msg_len);

    if (msg_len == 0) {
        payload_len = 0;
    } else {
        payload_len = (int) (msg_len / 4);
        if (msg_len % 4 != 0) {
            payload_len++;
        }
    }

    // Validate Size
    size_t frame_size = sizeof(struct mip_frame) + payload_len * 4;
    if (frame_size > MAX_MIP_FRAME_SIZE) {
        printf("MIP Frame exceeds max MIP frame size! Not sending frame!\n");
        return;
    }

    struct ethernet_frame eth_frame;
    uint8_t broadcast_addr[] = ETH_BROADCAST_ADDR;
    memcpy(eth_frame.dest, broadcast_addr, 6);
    memcpy(eth_frame.src, interface->sll_addr, 6);
    eth_frame.protocol = htons(ETH_P_MIP);

    printf("Payload Len: %d\n", payload_len);

    struct mip_frame m_frame;
    m_frame.tra = TRA_ROUTING_BIT;
    m_frame.ttl = MAX_TTL;
    m_frame.payload_len = payload_len;
    m_frame.dest = dest;
    m_frame.src = src;

    struct msghdr* msgh;
    struct iovec iov[3];
    iov[0].iov_base = &eth_frame;
    iov[0].iov_len = sizeof(struct ethernet_frame);
    iov[1].iov_base = &m_frame;
    iov[1].iov_len = sizeof(struct mip_frame);
    iov[2].iov_base = msg;
    iov[2].iov_len = msg_len;
    msgh = calloc(1, sizeof(struct msghdr));

    struct sockaddr_ll so_name;
    memcpy(&so_name, interface, sizeof(struct sockaddr_ll));
    memcpy(so_name.sll_addr, broadcast_addr, 6);

    msgh->msg_name = interface;
    msgh->msg_namelen = sizeof(struct sockaddr_ll);
    msgh->msg_iovlen = 3;
    msgh->msg_iov = iov;

    if (sendmsg(socket_fd, msgh, 0) == -1) {
        perror("sendmsg");
    }

    free(msgh);
}

int send_mip_frame(int socket_fd, struct sockaddr_ll* interface, unsigned char dest_mac[6], uint8_t src, uint8_t dest, char* msg) {
    // Calculate Payload Length
    int payload_len;
    int msg_len = strlen(msg);

    if (msg_len == 0) {
        payload_len = 0;
    } else {
        payload_len = (int) (msg_len / 4);
        if (msg_len % 4 != 0) {
            payload_len++;
        }
    }

    // Validate Size
    size_t frame_size = sizeof(struct mip_frame) + payload_len * 4;
    if (frame_size > MAX_MIP_FRAME_SIZE) {
        printf("MIP Frame exceeds max MIP frame size!\n");
        return MIP_CODE_OVERFLOW_SIZE;
    }
    
    struct ethernet_frame eth_frame;
    memcpy(eth_frame.dest, dest_mac, 6);
    memcpy(eth_frame.src, interface->sll_addr, 6);
    eth_frame.protocol = htons(ETH_P_MIP);

    struct mip_frame m_frame;
    m_frame.tra = TRA_TRANSPORT_BIT;
    m_frame.ttl = MAX_TTL;
    m_frame.payload_len = payload_len;
    m_frame.dest = dest;
    m_frame.src = src;
    
    struct msghdr* msgh;
    struct iovec iov[3];
    iov[0].iov_base = &eth_frame;
    iov[0].iov_len = sizeof(struct ethernet_frame);
    iov[1].iov_base = &m_frame;
    iov[1].iov_len = sizeof(struct mip_frame);
    iov[2].iov_base = msg;
    iov[2].iov_len = msg_len + 1;
    msgh = calloc(1, sizeof(struct msghdr));
    msgh->msg_name = interface;
    msgh->msg_namelen = sizeof(struct sockaddr_ll);
    msgh->msg_iovlen = 3;
    msgh->msg_iov = iov;

    if (sendmsg(socket_fd, msgh, 0) == -1) {
        perror("sendmsg");
        return MIP_CODE_LINK_DOWN;
    }

    free(msgh);
    return MIP_CODE_FRAME_SENT;
}
