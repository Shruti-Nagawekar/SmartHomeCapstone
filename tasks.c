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
            readCurrent(&data);
            // Sample voltage from first INA219 sensor
            readVoltage(&data);
            // Calculate power for first sensor
            data->total_power += (data->current * data->voltage);
            data->total_count++;
            state = SAMPLE;
            break;
    }
    data->state = state;
    return 0;
}

static void readCurrent(tasksAData* data) {
    // Placeholder for reading current from INA219 sensor
    int channel = data->channel;
    // Call function from INA219 to read current
    int current = 0; // INA219_ReadCurrent(channel);
    data->current = current;
    return;
}
static void readVoltage(tasksAData* data) {
    // Placeholder for reading voltage from INA219 sensor
    int channel = data->channel;
    // Call function from INA219 to read voltage
    int voltage = 0; // INA219_ReadVoltage(channel);
    data->voltage = voltage;
    return;
}