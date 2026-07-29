#ifndef OS_EVENT_H
#define OS_EVENT_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    volatile uint8_t flag;
    uint8_t waiting_task;
    uint8_t has_waiter;
} OSEvent;

// API
void os_event_init(OSEvent *e);
void os_event_signal(OSEvent *e);
void os_event_wait(OSEvent *e);
#endif
