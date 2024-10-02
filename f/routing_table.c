#include "routing_table.h"

void add_routing_entry(struct linked_list* routing_table, uint8_t dest, uint8_t next_hop, uint8_t cost) {
    struct routing_table_entry entry;
    entry.dest = dest;
    entry.next_hop = next_hop;
    entry.cost = cost;

    add_element(routing_table, &entry);
}

uint8_t get_next_hop(struct linked_list* routing_table, uint8_t mip_lookup) {
    if (routing_table->next == NULL) {
        return NO_ROUTE;
    }

    struct linked_list_node* cur_node = routing_table->next;
    while (cur_node != NULL) {
        struct routing_table_entry* routing_entry = (struct routing_table_entry*) cur_node->data;
        if (routing_entry->dest == mip_lookup) {
            return routing_entry->next_hop;
        }
        cur_node = cur_node->next;
    }

    return NO_ROUTE;
}

struct routing_table_entry* get_routing_entry(struct linked_list* routing_table, uint8_t dest) {
    if (routing_table->next == NULL) {
        return NULL;
    }

    struct linked_list_node* cur_node = routing_table->next;
    while (cur_node != NULL) {
        struct routing_table_entry* routing_entry = (struct routing_table_entry*) cur_node->data;
        if (routing_entry->dest == dest) {
            return routing_entry;
        }
        cur_node = cur_node->next;
    }

    return NULL;
}

void print_routing_table(struct linked_list* routing_table) {
    printf("Routing Table:\n");
    if (routing_table->next == NULL) {
        printf("No entries\n");
        return;
    }

    printf("Dest - Cost - Next Hop\n");
    struct linked_list_node* cur_node = routing_table->next;
    while (cur_node != NULL) {
        struct routing_table_entry* entry = (struct routing_table_entry*) cur_node->data;
        printf("%d - %d - %d\n", entry->dest, entry->cost, entry->next_hop);
        cur_node = cur_node->next;
    }
}
