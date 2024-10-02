#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <stdint.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <string.h>

#include "linked_list.h"

struct interface {
    struct sockaddr_ll data;
    uint8_t mip_address;
};

int enumerate_interfaces(struct linked_list* interfaces, int mip_addresses[], int num_mip_addresses, int debug);

struct interface* get_interface(struct linked_list* interfaces, int mip_address);

#endif
