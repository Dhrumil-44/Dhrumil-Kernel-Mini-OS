#include "os_event.h"
#include "os.h"
#include "cmsis_gcc.h"

void os_event_init(OSEvent *e) {
    e->flag = 0;
    e->has_waiter = 0;
    e->waiting_task = 0;
}

void os_event_signal(OSEvent *e) {
    __disable_irq();
    if (e->has_waiter) {
        e->has_waiter = 0;
        e->flag = 0;
        os_task_unblock(e->waiting_task);
    } else {
        e->flag = 1;
    }
    __enable_irq();
}

void os_event_wait(OSEvent *e) {
    __disable_irq();
    if (e->flag) {
        e->flag = 0;
        __enable_irq();
        return;
    }
    e->waiting_task = os_current_task();
    e->has_waiter = 1;
    __enable_irq();
    os_task_block();
}
