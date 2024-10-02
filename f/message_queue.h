#ifndef __MESSAGE_QUEUE_H
#define __MESSAGE_QUEUE_H

#include <stdint.h>

#include "ping.h"
#include "linked_list.h"

void queue_message(struct linked_list* queue, uint8_t dest, char message[]);

struct app_message* pop_message(struct linked_list* queue);

struct app_message* get_message(struct linked_list* queue);

#endif
