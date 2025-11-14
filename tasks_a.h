#ifndef TASKS_A_H
#define TASKS_A_H

typedef struct {
    int state;
    int channel;
    int current;
    int voltage;
    int total_power;
    int total_count;
    int avg_power;
} tasksAData;

enum A_States {
    INIT,
    SAMPLE
};

int A_Sample(tasksAData* data);

#endif // TASKS_A_H