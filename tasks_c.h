#ifndef TASKS_C_H
#define TASKS_C_H

#include "tasks_a.h"

typedef struct {
    int threshold;
} tasksCData;

void task_c(void *arg);

void task_c_set_source(tasksAData* source_data);

#endif // TASKS_C_H