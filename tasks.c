// Code to sample the data points at __ kHz
#include <stdio.h>
#include "tasks_a.h"

// Need to check if we are storing the average current/voltage/power over time

int A_Sample(tasksAData* data) {
    int state = data->state;
    switch (state) {
        case INIT:
            state = SAMPLE;
            break;
        case SAMPLE:
            // Sample current from first INA219 sensor
            readCurrent(data->channel);
            // Sample voltage from first INA219 sensor
            readVoltage(data->channel);
            // Calculate power for first sensor
            data->total_power += (data->current * data->voltage);
            data->total_count++;
            state = SAMPLE;
            break;
    }
}

static int readCurrent(int channel) {
    // Placeholder for reading current from INA219 sensor
    
    return 0;
}
static int readVoltage(int channel);