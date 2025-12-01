#ifndef RTOS_H
#define RTOS_H

#include <stdint.h>

#define MAX_TASKS 3

typedef enum {
    TASK_UNUSED,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
} TaskState;

typedef void (*TaskFn)(void *arg);

typedef struct {
    uint32_t *sp;       // Stack pointer
    TaskFn fn;    // Task function
    void *arg;          // Task argument
} task;

extern task g_tasks[MAX_TASKS];
extern int g_num_tasks;
extern int g_current_task;
extern task *g_current_task;

int rtos_create_task(TaskFn fn, void *arg, uint32_t *stack_mem, uint32_t stack_words);
void rtos_start(void);
void rtos_trigger_context_switch(void);

#endif // RTOS_H