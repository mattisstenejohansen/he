#include <string.h>
#include <stdint.h>

#include "linked_list.h"

struct linked_list* create_linked_list(size_t element_size) {
    struct linked_list* list = calloc(1, sizeof(struct linked_list));
    list->num_elements = 0;
    list->element_size = element_size;
    list->next = NULL;

    return list;
}

void add_element(struct linked_list* linked_list, void* element) {

    // Create New Node
    struct linked_list_node* node = calloc(1, sizeof(struct linked_list_node));
    node->data = calloc(1, linked_list->element_size);
    memcpy(node->data, element, linked_list->element_size);
    node->next = NULL;

    // Insert Element at end of list
    if (linked_list->next == NULL) {
        linked_list->next = node;
    } else {
        struct linked_list_node* current = linked_list->next;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = node;
    }

    linked_list->num_elements++;
}

void free_linked_list(struct linked_list* linked_list) {
    struct linked_list_node* current = linked_list->next;

    // Free Nodes and Data
    while (current != NULL) {
        struct linked_list_node* next = current->next;
        free(current->data);
        free(current);
        current = next;
    }

    // Free List
    free(linked_list);
}
