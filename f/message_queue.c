#include "message_queue.h"

#include <stdio.h>

void queue_message(struct linked_list* queue, uint8_t dest, char message[]) {
    struct app_message elem;
    memset(&elem, 0, sizeof(struct app_message));
    elem.dest = dest;
    memcpy(&elem.message, &message[0], strlen(message));
    printf("Queued message: %d - %s\n", elem.dest, elem.message);
    add_element(queue, &elem);
}

// User has to free returned message
struct app_message* pop_message(struct linked_list* queue) {
    if (queue->next == NULL) {
        return NULL;
    }

    printf("Popped Element from Queue\n");
    struct linked_list_node* cur_node = queue->next;
    queue->num_elements--;
    if (cur_node->next == NULL) {
        struct app_message* message = (struct app_message*) cur_node->data;
        queue->next = NULL;
        free(cur_node);
        return message;
    } else {
        struct linked_list_node* new_node = cur_node->next;
        struct app_message* message = (struct app_message*) cur_node->data;
        queue->next = new_node;
        free(cur_node);
        return message;
    }
}

struct app_message* get_message(struct linked_list* queue) {
    if (queue->next == NULL) {
        return NULL;
    }

    struct linked_list_node* cur_node = queue->next;
    struct app_message* message = (struct app_message*) cur_node->data;

    return message;
}
