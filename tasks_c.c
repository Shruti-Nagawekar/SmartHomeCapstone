#include "tasks_c.h"
//#include "main.h"

static tasksAData *g_source = 0;

void task_c_set_source(tasksAData* source_data) {
    g_source = source_data;
}

static void actuator_set(int state) {
    // Placeholder for setting actuator state
    if (state) {
        // Activate actuator
    } else {
        // Deactivate actuator
    }
    // For example, set a GPIO pin high or low
}

void task_c(void *arg) {
    tasksCData *cfg = (tasksCData *)arg;
    int last_state = -1;

    while(1) {
        if (g_source != 0) {
            int power = g_source->power;
            int new_state = (power > cfg->threshold) ? 1 : 0;

            if (new_state != last_state) {
                actuator_set(new_state);
                last_state = new_state;
            }
        }
    }
}