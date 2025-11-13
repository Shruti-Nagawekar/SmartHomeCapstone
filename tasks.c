// Code to sample the data points at __ kHz
#include <stdio.h>

// Need to check if we are storing the average current/voltage/power over time
typedef struct {
    int current;
    int voltage;
    int power;
    int total_current;
    int total_count;
    int avg_current;
} tasksA;

enum A_States {
    INIT,
    CURRENT0,
    CURRENT1,
    VOLTAGE0,
    VOLTAGE1
};

int TaskA(int state) {
    switch (state) {
        case INIT:
            break;
        case CURRENT0:
            // Sample current from first INA219 sensor
            // Add current to total_current and increment total_count
            break;
        case CURRENT1:
            // Sample current from second INA219 sensor
            // Add current to total_current and increment total_count
            break;
        case VOLTAGE0:
            // Sample voltage from first INA219 sensor
            break;
        case VOLTAGE1:
            // Sample voltage from second INA219 sensor
            break;
    }
}

static int readCurrent(int channel, int *output);
static int readVoltage(int channel, int *output);