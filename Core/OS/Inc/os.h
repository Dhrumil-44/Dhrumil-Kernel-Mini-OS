#ifndef OS_H
#define OS_H

#include <stdint.h>

// Task states
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED
} TaskState;

#define OS_MAX_TASKS  8
#define STACK_SIZE    512    // 256 x 4 = 1024 bytes per task

// Task Control Block
typedef struct {
    uint32_t *stack_ptr;
    uint8_t stack[STACK_SIZE];
    void (*func)(void);
    TaskState state;
    uint32_t wake_tick;
    const char *name;
} TCB;

// Scheduler API
void os_init(void);
void os_task_create(void (*func)(void), const char *name);
void os_start(void);
void os_yield(void);
void os_sleep_ms(uint32_t ms);
uint32_t os_now_ms(void);
void os_tick(void);

// Introspection
uint8_t os_task_count(void);
TaskState os_task_get_state(uint8_t index);
const char *os_task_get_name(uint8_t index);

// Internal (used by primitives)
void os_task_block(void);
void os_task_unblock(uint8_t task_index);
uint8_t os_current_task(void);

// Called by PendSV to pick next task
void os_schedule(void);

// Exposed for PendSV handler
extern TCB tasks[];
extern uint8_t current_task;
extern uint8_t os_started;
#endif
