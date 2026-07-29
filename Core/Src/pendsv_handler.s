    .syntax unified
    .cpu cortex-m4
    .thumb

    .extern os_schedule
    .extern current_task
    .extern tasks
    .extern os_started

    .global PendSV_Handler
    .type PendSV_Handler, %function

PendSV_Handler:
    @ skip save on very first switch
    LDR     R0, =os_started
    LDRB    R0, [R0]
    CBZ     R0, load_next

    @ Save current task context
    MRS     R0, PSP
    STMDB   R0!, {R4-R11}

    @ Store updated SP into tasks[current_task].stack_ptr (offset 0)
    LDR     R1, =current_task
    LDRB    R2, [R1]
    LDR     R3, =tasks
    MOV     R4, #532
    MUL     R2, R2, R4
    ADD     R3, R3, R2
    STR     R0, [R3]

load_next:
    @ Mark OS as started
    LDR     R0, =os_started
    MOV     R1, #1
    STRB    R1, [R0]

    @ Select next task
    PUSH    {LR}
    BL      os_schedule
    POP     {LR}

    @ Load next task context
    LDR     R1, =current_task
    LDRB    R2, [R1]
    LDR     R3, =tasks
    MOV     R4, #532
    MUL     R2, R2, R4
    ADD     R3, R3, R2
    LDR     R0, [R3]

    @ Restore R4-R11 and set PSP
    LDMIA   R0!, {R4-R11}
    MSR     PSP, R0

    @ Return using PSP
    ORR     LR, LR, #0x04
    BX      LR
