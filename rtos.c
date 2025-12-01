#include "rtos.h"

task g_tasks[MAX_TASKS];
int g_num_tasks = 0;
int g_current_task = -1;
task *g_current_task = 0;

static uint32_t *rtos_init_stack(TaskFn fn, void *arg, uint32_t *stack_top);
task *rtos_schedule_next(void);

int rtos_create_task(TaskFn fn, void *arg, uint32_t *stack_mem, uint32_t stack_words) {
    if (g_num_tasks >= MAX_TASKS) {
        return -1; // Max tasks reached
    }

    task *t = &g_tasks[g_num_tasks];

    uint32_t *stack_top = stack_mem + stack_words;
    stack_top = rtos_init_stack(fn, arg, stack_top);

    t->sp = stack_top;
    t->fn = fn;
    t->arg = arg;

    g_num_tasks++;
    return g_num_tasks - 1; // Success
}

void rtos_start(void) {
    g_current_task = 0;
    g_current_task = &g_tasks[g_current_task];

    // Set up the initial stack pointer and start the first task
    // This would typically involve assembly code to set the MSP/PSP and jump to the task function
}

void rtos_trigger_context_switch(void) {
    // Save current task context
    // This would typically involve assembly code to save registers onto the stack

    // Select next task
    g_current_task = rtos_schedule_next();

    // Restore next task context
    // This would typically involve assembly code to restore registers from the stack
}

static uint32_t *rtos_init_stack(TaskFn fn, void *arg, uint32_t *stack_top) {
    stack_top -= 8; // Make space for 8 registers

    // Simulate stack frame as it would be after an interrupt
    *(--stack_top) = (uint32_t)0x01000000; // xPSR
    *(--stack_top) = (uint32_t)fn;         // PC
    *(--stack_top) = (uint32_t)0xFFFFFFFD; // LR (Return to Thread mode with PSP)
    *(--stack_top) = (uint32_t)0x12121212; // R12
    *(--stack_top) = (uint32_t)0x03030303; // R3
    *(--stack_top) = (uint32_t)0x02020202; // R2
    *(--stack_top) = (uint32_t)0x01010101; // R1
    *(--stack_top) = (uint32_t)arg;        // R0 = arg

    return stack_top;
}

task *rtos_schedule_next(void) {
    if (g_num_tasks == 0) {
        return NULL; // No tasks to schedule
    }

    g_current_task++;
    if (g_current_task >= g_num_tasks) {
        g_current_task = 0; // Wrap around
    };

    return &g_tasks[g_current_task];
}

// Will need to setup PendSV for context switching