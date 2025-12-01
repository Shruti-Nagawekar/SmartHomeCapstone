// Code to sample the data points at 1 kHz
#include <stdint.h>
#include "tasks_a.h"
//#include "main.h"     // Main.h comes from STM32CubeMX generated code

// Need to check if we are storing the average current/voltage/power over time
// Also need to decide if we want 2 separate versions of task_a for each INA219
/*void task_a(void *arg) {
    tasksAData* data = (tasksAData*)arg;
    // Reset values
    Semaphore sem_tasksA;
    sem_init(&sem_tasksA, 0);
    
    int state = data->state;
    data->current = 0;
    data->voltage = 0;
    data->total_power = 0;
    data->total_count = 0;
    data->avg_power = 0;

    while (1) {
        // Wait until timer ISR
        sem_wait(&sem_tasksA);

        data->current = readCurrent(data->channel);
        data->voltage = readVoltage(data->channel);

        int power = data->current * data->voltage;
        data->total_power += power;
        data->total_count++;
        data->avg_power = data->total_power / data->total_count;
    }
}*/

int readCurrent(int channel) {
    // Placeholder for reading current from INA219 sensor

    // Call function from INA219 to read current
    int current = 0; // INA219_ReadCurrent(channel);

    return current;
}
int readVoltage(int channel) {
    // Placeholder for reading voltage from INA219 sensor

    // Call function from INA219 to read voltage
    int voltage = 0; // INA219_ReadVoltage(channel);

    return voltage;
}

void task_a(void *arg) {
    tasksAData* data = (tasksAData*)arg;
    uint32_t last_tick = g_tick_ms;

    // Reset values
    data->current = 0;
    data->voltage = 0;
    data->total_power = 0;
    data->total_count = 0;
    data->avg_power = 0;

    while (1) {
        uint32_t current_tick = g_tick_ms;
        if (current_tick != last_tick) {
            last_tick = current_tick;
            
            // Sample data every 1 ms
            data->current = readCurrent(data->channel);
            data->voltage = readVoltage(data->channel);
            data->power = data->current * data->voltage;
            data->total_power += data->power;
            data->total_count++;
            data->avg_power = data->total_power / data->total_count;

        }
    }
}