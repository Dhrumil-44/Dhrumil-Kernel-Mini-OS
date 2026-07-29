#ifndef OS_QUEUE_H
#define OS_QUEUE_H
#include <stdint.h>
#include <stdbool.h>

#define QUEUE_MAX_ITEMS  8
#define QUEUE_ITEM_SIZE  16   // max bytes per message

typedef struct {
    uint8_t buffer[QUEUE_MAX_ITEMS][QUEUE_ITEM_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} OSQueue;

// API
void os_queue_init(OSQueue *q);
bool os_queue_send(OSQueue *q, const void *item, uint8_t size);
bool os_queue_recv(OSQueue *q, void *item, uint8_t size);
bool os_queue_is_empty(OSQueue *q);
bool os_queue_is_full(OSQueue *q);

#endif
