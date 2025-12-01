#include <stdint.h>
#include "rtos.h"
#include "tasks_a.h"
#include "tasks_c.h"

extern TIM_HandleTypeDef htim2;

// 1 ms tick counter
volatile uint32_t g_tick_ms = 0;

// Task data structures
tasksAData g_taskA_data;
tasksCData g_taskC_data;

// 128 word stacks for each task
uint32_t g_taskA_stack[128];
uint32_t g_taskC_stack[128];
uint32_t g_idle_stack[128];     // Configure to task B eventually

// Timer interrupt from STM32 Library
void HAL_TIM_PeriodElapsedCallback(void *htim) {
    // This function is called every 1 ms by the timer interrupt
    if (htim->Instance == TIM2) {
        g_tick_ms++;
        rtos_trigger_context_switch();
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();     // Initialize I2C for INA219
    MX_TIM2_Init();     // Configure timer 2 for 1 ms ticks

    HAL_TIM_Base_Start_IT(&htim2);  // Start timer with interrupts

    // Create tasks
    g_taskA_data.channel = 0;       // Sensor 0
    g_taskC_data.threshold = 1000;  // Example threshold
    task_c_set_source(&g_taskA_data);
    
    rtos_create_task(task_a, &g_taskA_data, g_taskA_stack, 128);
    rtos_create_task(task_c, &g_taskC_data, g_taskC_stack, 128);
    rtos_create_task(idle_task, NULL, g_idle_stack, 128);

    rtos_start();  // Start the RTOS scheduler

    while (1) {
        // Should never reach here
    }
}