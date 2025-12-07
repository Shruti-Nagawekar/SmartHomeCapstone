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

/* ===================== Globals & Typedefs ===================== */

/* (Scheduler types are still here but not used yet) */
typedef void (*task_fn_t)(void);

typedef struct {
    task_fn_t  fn;
    uint32_t   period_ms;
    uint32_t   next_release;
} task_t;

#define NUM_TASKS 3

static task_t tasks[NUM_TASKS];
static volatile uint32_t sys_ticks  = 0;
static volatile int      sched_flag = 0;

/* Forward declarations of tasks */
void TaskSense(void);
void TaskControl(void);
void TaskComms(void);

/* ===================== SysTick (just for timebase) ===================== */

/* HAL calls this every 1 ms by default */
void HAL_SYSTICK_Callback(void)
{
    sys_ticks++;
    sched_flag = 1;   // not used yet, but harmless
}

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

/* ===================== Tasks ===================== */

static volatile uint8_t fan_on = 0;

void TaskSense(void)
{
    uint16_t a, b;
    sensor_read(&a, &b);   // read into locals
    powerA = a;            // then update the volatile globals
    powerB = b;
}

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
}

void TaskComms(void)
{
    /* Use sys_ticks as our time base (incremented in HAL_SYSTICK_Callback) */
    comms_send(sys_ticks, powerA, powerB, fan_on);
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
    MX_GPIO_Init();          // LED pin
    MX_USART2_UART_Init();   // UART2 on STLink VCP

    const char *start_msg = "Simple 3-task demo start\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)start_msg,
                      strlen(start_msg), HAL_MAX_DELAY);

    while (1)
    {
        /* Run our three “tasks” once every 500 ms */
        TaskSense();    // read fake sensor
        TaskControl();  // update fan + LED
        TaskComms();    // print values

        HAL_Delay(500);
    }
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
