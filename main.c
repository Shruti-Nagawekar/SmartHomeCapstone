#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Function prototypes from CubeMX */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

/* UART handle (used by HAL + printf) */
UART_HandleTypeDef huart2;
I2C_HandleTypeDef  hi2c1;   // I2C handle for INA219

/* ===================== INA219 Driver ===================== */
/* I2C 7-bit addresses (shifted left by 1 for HAL) */
#define INA219_FAN_ADDR      (0x40 << 1)   // adjust according to A0/A1 wiring
#define INA219_PHONE_ADDR    (0x41 << 1)   // second INA219

/* INA219 registers */
#define INA219_REG_CONFIG    0x00
#define INA219_REG_SHUNT     0x01
#define INA219_REG_BUS       0x02
#define INA219_REG_CALIB     0x05

static HAL_StatusTypeDef ina219_write_reg(uint16_t devAddr, uint8_t reg, uint16_t value);
static HAL_StatusTypeDef ina219_read_reg(uint16_t devAddr, uint8_t reg, uint16_t *value);
static void              ina219_init(uint16_t devAddr);
static uint16_t          ina219_read_power_mW(uint16_t devAddr);

/* ===================== INA219 Functions ===================== */

static void ina219_init(uint16_t devAddr)
{
    uint16_t config = 0x399F;  // default from datasheet

    HAL_StatusTypeDef st = ina219_write_reg(devAddr, INA219_REG_CONFIG, config);

    if (st != HAL_OK) {
        printf("INA219 init FAILED at addr 0x%02lX (status=%d)\r\n",
               (unsigned long)(devAddr >> 1), st);
    } else {
        printf("INA219 init OK at addr 0x%02lX\r\n",
               (unsigned long)(devAddr >> 1));
    }
}

static HAL_StatusTypeDef ina219_write_reg(uint16_t devAddr, uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    data[0] = (uint8_t)(value >> 8);       // MSB
    data[1] = (uint8_t)(value & 0xFF);     // LSB

    // was: HAL_MAX_DELAY
    return HAL_I2C_Mem_Write(&hi2c1, devAddr,
                             reg, I2C_MEMADD_SIZE_8BIT,
                             data, 2, 20);   // 20 ms timeout
}

static HAL_StatusTypeDef ina219_read_reg(uint16_t devAddr, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(&hi2c1, devAddr,
                              reg, I2C_MEMADD_SIZE_8BIT,
                              data, 2, 20);   // 20 ms timeout
    if (status != HAL_OK) return status;

    *value = ((uint16_t)data[0] << 8) | data[1];
    return HAL_OK;
}

static uint16_t ina219_read_power_mW(uint16_t devAddr)
{
    uint16_t raw_shunt_u16;
    uint16_t raw_bus_u16;

    if (ina219_read_reg(devAddr, INA219_REG_SHUNT, &raw_shunt_u16) != HAL_OK)
        return 0;
    if (ina219_read_reg(devAddr, INA219_REG_BUS, &raw_bus_u16) != HAL_OK)
        return 0;

    /* Shunt voltage is signed (two's complement) */
    int16_t raw_shunt = (int16_t)raw_shunt_u16;

    int32_t current_mA = (int32_t)raw_shunt;  // in units of 0.1 mA
    current_mA = current_mA / 10;             // ≈ mA

    if (current_mA < 0) current_mA = -current_mA;  // absolute value

    /* Bus voltage:
     * bus_reg LSB = 4 mV, bits 0-2 are flags → shift right by 3.
     */
    uint32_t bus_mV = ((uint32_t)(raw_bus_u16 >> 3)) * 4;

    /* Calculate power converted to mW */
    uint32_t p_uW = bus_mV * (uint32_t)current_mA;
    uint32_t p_mW = p_uW / 1000;

    if (p_mW > 0xFFFF) p_mW = 0xFFFF;   // clamp to 16-bit

    return (uint16_t)p_mW;
}

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

static void sensor_ina219(uint16_t *pA, uint16_t *pB)
{
    uint16_t p_fan_mW   = ina219_read_power_mW(INA219_FAN_ADDR);
    uint16_t p_phone_mW = ina219_read_power_mW(INA219_PHONE_ADDR);

    *pA = p_fan_mW;
    *pB = p_phone_mW;
}

static sensor_read_fn_t sensor_read = sensor_ina219;

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
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, s);

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
 * - Called from SysTick handler in Cube IDE Config Files
 */
void scheduler(void)
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
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock (CubeMX-generated) */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();          // Enable I2C for INA219

    /* Initialize INA219 sensors */
    ina219_init(INA219_FAN_ADDR);       // Initialize the fan sensor
    ina219_init(INA219_PHONE_ADDR);     // Initialize the phone charger sensor

    const char *start_msg = "RTOS-style 3-task demo start\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)start_msg,
                      strlen(start_msg), HAL_MAX_DELAY);

    /* Initialize the software RTOS scheduler */
    init_tasks();

    while (1)
    {
        scheduler();
        HAL_Delay(1);
    }
}

/* ===================== CubeMX-style init functions ===================== */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin (User Button) */
  GPIO_InitStruct.Pin  = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin (LED) */
  GPIO_InitStruct.Pin   = LD2_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
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
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing           = 0x00303D5B;   // from CubeMX for ~100 kHz @ your clock
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* ===================== Error Handler ===================== */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    /* Optional: blink LED rapidly to signal error */
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
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
