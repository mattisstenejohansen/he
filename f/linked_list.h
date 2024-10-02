#ifndef __LIST_H
#define __LIST_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// Generic Linked List
struct linked_list {
    uint8_t num_elements;
    size_t element_size;
    struct linked_list_node* next;
};

// Generic Linked List Node
struct linked_list_node {
    void* data;
    struct linked_list_node* next;
};

struct linked_list* create_linked_list(size_t element_size);

void add_element(struct linked_list* linked_list, void* element);

void free_linked_list(struct linked_list* linked_list);

#endif
