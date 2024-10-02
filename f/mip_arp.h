#ifndef __MIP_ARP_H
#define __MIP_ARP_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "linked_list.h"

// MIP-ARP Entry
struct mip_arp_entry {
    uint8_t mip_address;
    uint8_t mac_address[6];
    uint8_t via_interface;
} __attribute__ ((packed));

struct mip_arp_entry* get_mip_arp_entry(struct linked_list* arp_cache, uint8_t mip_address);

void add_mip_arp_entry(struct linked_list* arp_cache, uint8_t mip, unsigned char mac[6], uint8_t via_interface);
#endif
