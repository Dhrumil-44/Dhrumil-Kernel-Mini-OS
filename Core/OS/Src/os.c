#include "os.h"
#include "cmsis_gcc.h"
#include "stm32f4xx.h"
#include "core_cm4.h"

TCB     tasks[OS_MAX_TASKS];
uint8_t current_task = 0;
static uint8_t  task_count  = 0;
static uint32_t os_ticks    = 0;
uint8_t os_started = 0;

void os_init(void) {
    task_count   = 0;
    current_task = 0;
    os_ticks     = 0;
    os_started   = 0;

    NVIC_SetPriority(PendSV_IRQn,  0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);
}


// Pre filled task stack to ensure PendSV restore works correctly
static void os_task_stack_init(TCB *task) {
    uint32_t *top = (uint32_t*)(task->stack + STACK_SIZE);
    top = (uint32_t*)((uint32_t)top & ~0x7UL);
    *(--top) = 0x01000000;           // xPSR — Thumb bit must be set
    *(--top) = (uint32_t)task->func; // PC   — task entry point
    *(--top) = 0xFFFFFFFD;           // LR   — EXC_RETURN (PSP, no FPU)
    *(--top) = 0x12121212;           // R12
    *(--top) = 0x03030303;           // R3
    *(--top) = 0x02020202;           // R2
    *(--top) = 0x01010101;           // R1
    *(--top) = 0x00000000;           // R0

    *(--top) = 0x11111111;           // R11
    *(--top) = 0x10101010;           // R10
    *(--top) = 0x09090909;           // R9
    *(--top) = 0x08080808;           // R8
    *(--top) = 0x07070707;           // R7
    *(--top) = 0x06060606;           // R6
    *(--top) = 0x05050505;           // R5
    *(--top) = 0x04040404;           // R4

    task->stack_ptr = top;
}

void os_task_create(void (*func)(void), const char *name) {
    if (task_count >= OS_MAX_TASKS) return;

    tasks[task_count].func      = func;
    tasks[task_count].state     = TASK_READY;
    tasks[task_count].wake_tick = 0;
    tasks[task_count].name      = name;

    os_task_stack_init(&tasks[task_count]);

    task_count++;
}

uint32_t os_now_ms(void) {
    return os_ticks;
}

void os_tick(void) {
    os_ticks++;
    for (uint8_t i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_SLEEPING) {
            if (os_ticks >= tasks[i].wake_tick) {
                tasks[i].state = TASK_READY;
            }
        }
    }
}
void os_schedule(void) {
    if (tasks[current_task].state == TASK_RUNNING) {
        tasks[current_task].state = TASK_READY;
    }
    uint8_t next = current_task;
    for (uint8_t i = 1; i <= task_count; i++) {
        uint8_t idx = (current_task + i) % task_count;
        if (tasks[idx].state == TASK_READY) {
            next = idx;
            break;
        }
    }

    current_task = next;
    tasks[current_task].state = TASK_RUNNING;
}

void os_yield(void) {
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __ISB();
}

void os_sleep_ms(uint32_t ms) {
    __disable_irq();
    tasks[current_task].state = TASK_SLEEPING;
    tasks[current_task].wake_tick = os_ticks + ms;
    __enable_irq();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __ISB();
}

void os_task_block(void) {
    __disable_irq();
    tasks[current_task].state = TASK_BLOCKED;
    __enable_irq();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __ISB();
}

void os_task_unblock(uint8_t task_index) {
    if (tasks[task_index].state == TASK_BLOCKED) {
        tasks[task_index].state = TASK_READY;
    }
}

uint8_t os_current_task(void) {
    return current_task;
}

uint8_t os_task_count(void) {
    return task_count;
}

TaskState os_task_get_state(uint8_t index) {
    if (index >= task_count) return TASK_READY;
    return tasks[index].state;
}

const char *os_task_get_name(uint8_t index) {
    if (index >= task_count) return "?";
    return tasks[index].name;
}
