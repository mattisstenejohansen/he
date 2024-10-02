#ifndef __ROUTING_TABLE_H
#define __ROUTING_TABLE_H

#include <stdint.h>

#include "linked_list.h"

#define NO_ROUTE 255

struct routing_table_entry {
    uint8_t dest;
    uint8_t next_hop;
    uint8_t cost;
};

void add_routing_entry(struct linked_list* routing_table, uint8_t dest, uint8_t next_hop, uint8_t cost);

uint8_t get_next_hop(struct linked_list* routing_table, uint8_t mip_lookup);

struct routing_table_entry* get_routing_entry(struct linked_list* routing_table, uint8_t dest);

void print_routing_table(struct linked_list* routing_table);

#endif
