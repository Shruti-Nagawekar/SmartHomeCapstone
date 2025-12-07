#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Function prototypes from CubeMX */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* UART handle (used by HAL + printf) */
UART_HandleTypeDef huart2;

/* ===================== Task & Scheduler Types ===================== */

/* Simple periodic task type */
typedef void (*task_fn_t)(void);

typedef struct {
    task_fn_t  fn;           // task function
    uint32_t   period_ms;    // period in ms
    uint32_t   next_release; // next release time in ms
} task_t;

#define NUM_TASKS 3

static task_t tasks[NUM_TASKS];

/* Forward declarations of tasks */
void TaskSense(void);
void TaskControl(void);
void TaskComms(void);

/* ===================== Sensor Abstraction ===================== */

typedef void (*sensor_read_fn_t)(uint16_t *powerA, uint16_t *powerB);

static volatile uint16_t powerA = 0;
static volatile uint16_t powerB = 0;

/* Simulated sensor – replace with INA219 later */
static void sensor_simulated(uint16_t *pA, uint16_t *pB)
{
    static uint16_t x = 0;
    x = (x + 5) % 1000;    // ramp 0..995 in steps of 5
    *pA = x;
    *pB = 1000 - x;
}

/* Real INA219 version will live here later
static void sensor_ina219(uint16_t *pA, uint16_t *pB)
{
    *pA = ina219_read_power(INA219_ADDR_1);
    *pB = ina219_read_power(INA219_ADDR_2);
}
*/

static sensor_read_fn_t sensor_read = sensor_simulated;

/* ===================== Comms Abstraction ===================== */

typedef void (*comms_send_fn_t)(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan);

/* Default: just printf over UART2 */
static void comms_uart(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan)
{
    printf("t=%lums pA=%u pB=%u fan=%u\r\n",
           (unsigned long)ticks, pA, pB, fan);
}

/* Later: ESP-AT Wi-Fi JSON etc.
static void comms_esp_at(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan)
{
}
*/

static comms_send_fn_t comms_send = comms_uart;

/* ===================== Inter-task Communication (Mailbox) ===================== */
/* Control task produces messages; Comms task consumes them. */

typedef struct {
    uint8_t  full;   // 1 = new data available
    uint32_t ticks;
    uint16_t pA;
    uint16_t pB;
    uint8_t  fan;
} comms_mailbox_t;

static volatile comms_mailbox_t comms_mailbox = {0};

/* ===================== Tasks ===================== */

static volatile uint8_t fan_on = 0;

/* TaskSense: reads sensors and updates shared power variables */
void TaskSense(void)
{
    uint16_t a, b;
    sensor_read(&a, &b);   // read into locals
    powerA = a;            // then update the volatile globals
    powerB = b;
}

/* TaskControl: applies threshold logic and updates LED + mailbox */
void TaskControl(void)
{
    const uint16_t THRESH = 600;
    GPIO_PinState s = GPIO_PIN_RESET;

    if (powerA > THRESH || powerB > THRESH) {
        fan_on = 1;
        s = GPIO_PIN_SET;
    } else {
        fan_on = 0;
    }

    /* Drive LD2 as our “fan” indicator */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, s);

    /* Publish a message to the mailbox for the Comms task */
    comms_mailbox.ticks = HAL_GetTick();
    comms_mailbox.pA    = powerA;
    comms_mailbox.pB    = powerB;
    comms_mailbox.fan   = fan_on;
    comms_mailbox.full  = 1;    // mark as new data
}

/* TaskComms: reads mailbox and prints a message if new data is available */
void TaskComms(void)
{
    if (comms_mailbox.full) {
        /* Take snapshot */
        uint32_t ticks = comms_mailbox.ticks;
        uint16_t pA    = comms_mailbox.pA;
        uint16_t pB    = comms_mailbox.pB;
        uint8_t  fan   = comms_mailbox.fan;

        comms_mailbox.full = 0;  // consume message

        comms_send(ticks, pA, pB, fan);
    }
}

/* ===================== Scheduler ===================== */

/* Cooperative, time-based scheduler using HAL_GetTick()
 * - Runs in main context
 * - Chooses which task to run based on period & next_release time
 */
static void scheduler(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < NUM_TASKS; i++) {
        if ((int32_t)(now - tasks[i].next_release) >= 0) {
            tasks[i].fn();
            tasks[i].next_release += tasks[i].period_ms;
        }
    }
}

/* Initialise the task table:
 * - Sense   @ 1 ms   (1 kHz)
 * - Control @ 10 ms  (100 Hz)
 * - Comms   @ 500 ms (2 Hz)
 */
static void init_tasks(void)
{
    tasks[0] = (task_t){ TaskSense,   1,   1   };
    tasks[1] = (task_t){ TaskControl, 10,  10  };
    tasks[2] = (task_t){ TaskComms,   500, 500 };
}

/* ===================== printf → UART2 ===================== */

/* GCC / newlib: retarget _write so printf uses UART2 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* ===================== main() ===================== */

int main(void)
{
    HAL_Init();              // init HAL, SysTick, etc.

    SystemClock_Config();    // clocks
    MX_GPIO_Init();          // LED & button pins
    MX_USART2_UART_Init();   // UART2 on STLink VCP

    const char *start_msg = "RTOS-style 3-task demo start\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)start_msg,
                      strlen(start_msg), HAL_MAX_DELAY);

    /* Initialize the software RTOS scheduler */
    init_tasks();

    while (1)
    {
        __WFI();   // Wait For Interrupt (low-power idle)
    }
}

/* ===================== SysTick Handler (preemptive scheduler hook) ===================== */

void SysTick_Handler(void)
{
    HAL_IncTick();   // HAL time base for HAL_GetTick, HAL_Delay, etc.

    scheduler();     // run our RTOS scheduler preemptively every tick
}

/* ===================== CubeMX-style init functions ===================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /* Initializes the RCC Oscillators */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK     |
                                       RCC_CLOCKTYPE_SYSCLK   |
                                       RCC_CLOCKTYPE_PCLK1    |
                                       RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    /*Configure GPIO pin : PC13 (User Button) */
    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pin : PA5 (LD2 LED) */
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/* ===================== Error Handler ===================== */

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* Optional: blink LED rapidly to signal error */
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(100);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    /* You can printf here if you want */
}
#endif
