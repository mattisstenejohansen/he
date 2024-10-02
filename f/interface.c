#include "interface.h"

int enumerate_interfaces(struct linked_list* interfaces, int mip_addresses[], int num_mip_addresses, int debug) {
    struct ifaddrs* ifaces;
    struct ifaddrs* ifp;
    
    getifaddrs(&ifaces);

    int i = 0;
    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
        if (ifp->ifa_addr != NULL & ifp->ifa_addr->sa_family == AF_PACKET) {
            // Check Bounds ?

            // Skip Loopback
            if (strcmp("lo", ifp->ifa_name) == 0) {
                if (debug) printf("Skipped loopback interface\n");
                continue;
            }

            // Add Interface
            struct interface interface;
            memcpy(&interface.data, (struct sockaddr_ll*) ifp->ifa_addr, sizeof(struct sockaddr_ll));
            interface.mip_address = mip_addresses[i];
            add_element(interfaces, &interface);
            
            if (debug) printf("Added Interface: %s\n", ifp->ifa_name);
            i++;
        }
    }

    free(ifaces);
    return i;
}

struct interface* get_interface(struct linked_list* interfaces, int mip_address) {
    if (interfaces->next == NULL) {
        return NULL;
    }

    struct linked_list_node* cur_node = interfaces->next;
    while (cur_node != NULL) {
        struct interface* interface = (struct interface*) cur_node->data;
	if (interface->mip_address == mip_address) {
            return interface;
        }
        cur_node = cur_node->next;
    }

    return NULL;
}
