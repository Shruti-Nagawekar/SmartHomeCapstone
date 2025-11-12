// Code to sample the data points at __ kHz
#include <stdio.h>

typedef struct {
    int sampling_rate;
    int state;
    unsigned char source;  // Determines which INA219 to read from
} tasksA;

enum A_States {
    INIT,
    SAMPLE
};

int TaskA(int state) {
    switch (state) {
        case INIT:
            break;
        case SAMPLE:
            break;
    }
}