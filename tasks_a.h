#ifndef TASKS_A_H
#define TASKS_A_H

typedef struct {
    int channel;
    int current;
    int voltage;
    int power;
    int total_power;
    int total_count;
    int avg_power;
} tasksAData;

void task_a(void *arg);

int readCurrent(int channel);
int readVoltage(int channel);

#endif // TASKS_A_H