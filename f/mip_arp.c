#include "mip_arp.h"

void add_mip_arp_entry(struct linked_list* arp_cache, uint8_t mip, unsigned char mac[6], uint8_t via_interface) {
    struct mip_arp_entry entry;
    entry.mip_address = mip;
    memcpy(entry.mac_address, mac, 6);
    entry.via_interface = via_interface;

    add_element(arp_cache, &entry);
}

struct mip_arp_entry* get_mip_arp_entry(struct linked_list* arp_cache, uint8_t mip_address) {
    if (arp_cache->next == NULL) {
        return NULL;
    }

    struct linked_list_node* cur_node = arp_cache->next;
    while (cur_node != NULL) {
        struct mip_arp_entry* arp_entry = (struct mip_arp_entry*) cur_node->data;
        if (arp_entry->mip_address == mip_address) {
            return arp_entry;
        }
        cur_node = cur_node->next;
    }

    return NULL;
}
