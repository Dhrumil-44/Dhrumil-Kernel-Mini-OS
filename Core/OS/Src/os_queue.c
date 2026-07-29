#include "os_queue.h"
#include <string.h>
#include "cmsis_gcc.h"

void os_queue_init(OSQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool os_queue_is_empty(OSQueue *q) {
    return (q->count == 0);
}

bool os_queue_is_full(OSQueue *q) {
    return (q->count >= QUEUE_MAX_ITEMS);
}

bool os_queue_send(OSQueue *q, const void *item, uint8_t size) {
    if (size > QUEUE_ITEM_SIZE) return false;
    __disable_irq();
    if (os_queue_is_full(q)) {
        __enable_irq();
        return false;
    }

    memcpy(q->buffer[q->tail], item, size);
    q->tail  = (q->tail + 1) % QUEUE_MAX_ITEMS;
    q->count++;
    __enable_irq();
    return true;
}

bool os_queue_recv(OSQueue *q, void *item, uint8_t size) {
    if (size > QUEUE_ITEM_SIZE) return false;
    __disable_irq();
    if (os_queue_is_empty(q)) {
        __enable_irq();
        return false;
    }
    memcpy(item, q->buffer[q->head], size);
    q->head  = (q->head + 1) % QUEUE_MAX_ITEMS;
    q->count--;
    __enable_irq();
    return true;
}
