// Creating RTOS
// Timer for 1 kHz tick

#include <stdint.h>
#include <stdio.h>

static volatile uint32_t tick_count = 0;
static void (*s_fn)(void) = 0;     // Function pointer to know what function was called

void tick_init(void (*fn)(void)) {
    s_fn = fn;
    // Start timer here
}

uint32_t tick_get(void) {
    return tick_count;
}

// When the timer interrupt occurs
void tick_isr(void) {
    tick_count++;
    if (s_fn) {
        s_fn();
    }
}

// Task 1 Sampling Data at 1 kHz

// Task 2

// Task 3 Measure data and handle actuators