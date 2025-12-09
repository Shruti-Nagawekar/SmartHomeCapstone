/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"   // if you don't need USB, you can remove this

#include <stdint.h>
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

/* ===================== INA219 Driver ===================== */

/* 7-bit base addresses (from A0/A1 pins on the breakout) */
#define INA1_ADDR_7BIT   0x40   // fan sensor
#define INA2_ADDR_7BIT   0x41   // second sensor

/* HAL expects 8-bit "addr << 1" form */
#define INA219_FAN_ADDR    (INA1_ADDR_7BIT << 1)
#define INA219_PHONE_ADDR  (INA2_ADDR_7BIT << 1)

/* INA219 registers */
#define INA219_REG_CONFIG   0x00
#define INA219_REG_SHUNT    0x01   // shunt voltage
#define INA219_REG_BUS      0x02   // bus voltage
#define INA219_REG_POWER    0x03
#define INA219_REG_CURRENT  0x04
#define INA219_REG_CALIB    0x05

static HAL_StatusTypeDef ina219_write_reg(uint16_t devAddr, uint8_t reg, uint16_t value);
static HAL_StatusTypeDef ina219_read_reg(uint16_t devAddr, uint8_t reg, uint16_t *value);
static void              ina219_init(uint16_t devAddr);
static uint16_t          ina219_read_power_mW(uint16_t devAddr);

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

/* Sensor abstraction */
typedef void (*sensor_read_fn_t)(uint16_t *powerA, uint16_t *powerB);

static volatile uint16_t powerA = 0;
static volatile uint16_t powerB = 0;
static sensor_read_fn_t  sensor_read;

/* Comms abstraction */
typedef void (*comms_send_fn_t)(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan);
static comms_send_fn_t  comms_send;

/* Mailbox between Control and Comms */
typedef struct {
    uint8_t  full;   // 1 = new data available
    uint32_t ticks;
    uint16_t pA;
    uint16_t pB;
    uint8_t  fan;
} comms_mailbox_t;

static volatile comms_mailbox_t comms_mailbox = {0};

static volatile uint8_t fan_on = 0;


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);

void scheduler(void);
static void init_tasks(void);
static void sensor_ina219(uint16_t *pA, uint16_t *pB);
static void comms_uart(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan);
static void I2C_Scan(void);

/* printf -> UART2 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* ===== INA219 low-level R/W ===== */

static HAL_StatusTypeDef ina219_write_reg(uint16_t devAddr, uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    data[0] = (uint8_t)(value >> 8);       // MSB
    data[1] = (uint8_t)(value & 0xFF);     // LSB

    return HAL_I2C_Mem_Write(&hi2c1,
                             devAddr,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             data,
                             2,
                             HAL_MAX_DELAY);
}

static HAL_StatusTypeDef ina219_read_reg(uint16_t devAddr, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(&hi2c1,
                              devAddr,
                              reg,
                              I2C_MEMADD_SIZE_8BIT,
                              data,
                              2,
                              HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    *value = ((uint16_t)data[0] << 8) | data[1];
    return HAL_OK;
}

/* ===== INA219 init (CONFIG + CALIB) ===== */

static void ina219_init(uint16_t devAddr)
{
    /* Example: currentLSB = 0.0001 A (100 µA/LSB), Rshunt = 0.1 Ω */
    float currentLSB_A = 0.0001f;
    uint16_t calib = (uint16_t)(0.04096f / (currentLSB_A * 0.1f));

    /* Write config and calibration registers */
    ina219_write_reg(devAddr, INA219_REG_CONFIG, 0x399F);
    ina219_write_reg(devAddr, INA219_REG_CALIB, calib);
}

/* ===================== Power ===================== */

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

    int32_t current_mA = (int32_t)raw_shunt;  // your approximation
    current_mA = current_mA / 10;             // ≈ mA

    if (current_mA < 0) current_mA = -current_mA;

    /* Bus voltage: LSB = 4 mV, bits 0-2 are flags → >>3 */
    uint32_t bus_mV = ((uint32_t)(raw_bus_u16 >> 3)) * 4;

    /* Power in µW, then mW */
    uint32_t p_uW = bus_mV * (uint32_t)current_mA;
    uint32_t p_mW = p_uW / 1000;

    if (p_mW > 0xFFFF) p_mW = 0xFFFF;

    return (uint16_t)p_mW;
}

/* ========== I2C Scanner ========== */

static void I2C_Scan(void)
{
    printf("\r\nScanning I2C bus...\r\n");

    for (uint8_t addr = 1; addr < 127; addr++) {
        hi2c1.ErrorCode = HAL_I2C_ERROR_NONE;
        HAL_StatusTypeDef res = HAL_I2C_IsDeviceReady(&hi2c1,
                                                      addr << 1,
                                                      2,
                                                      10);
        if (res == HAL_OK) {
            printf("  [OK] device at 0x%02X\r\n", addr);
        }
        HAL_Delay(2);
    }

    printf("Scan done.\r\n");
}

/* ========== Sensor abstraction ========== */

static void sensor_ina219(uint16_t *pA, uint16_t *pB)
{
    uint16_t p_fan_mW   = ina219_read_power_mW(INA219_FAN_ADDR);
    uint16_t p_phone_mW = ina219_read_power_mW(INA219_PHONE_ADDR);

    *pA = p_fan_mW;
    *pB = p_phone_mW;
}

/* ========== Comms abstraction ========== */

static void comms_uart(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan)
{
    printf("t=%lums pA=%u pB=%u fan=%u\r\n",
           (unsigned long)ticks, pA, pB, fan);
}

/* ========== Tasks ========== */

void TaskSense(void)
{
    uint16_t a, b;
    sensor_read(&a, &b);
    powerA = a;
    powerB = b;
}

void TaskControl(void)
{
    const uint16_t THRESH = 400;
    GPIO_PinState s = GPIO_PIN_RESET;

    if (powerA > THRESH || powerB > THRESH) {
        fan_on = 1;
        s = GPIO_PIN_SET;
    } else {
        fan_on = 0;
    }

    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, s);

    comms_mailbox.ticks = HAL_GetTick();
    comms_mailbox.pA    = powerA;
    comms_mailbox.pB    = powerB;
    comms_mailbox.fan   = fan_on;
    comms_mailbox.full  = 1;
}

void TaskComms(void)
{
    if (comms_mailbox.full) {
        uint32_t ticks = comms_mailbox.ticks;
        uint16_t pA    = comms_mailbox.pA;
        uint16_t pB    = comms_mailbox.pB;
        uint8_t  fan   = comms_mailbox.fan;

        comms_mailbox.full = 0;
        comms_send(ticks, pA, pB, fan);
    }
}

/* ========== Scheduler ========== */

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

static void init_tasks(void)
{
    tasks[0] = (task_t){ TaskSense,   1,   1   };
    tasks[1] = (task_t){ TaskControl, 10,  10  };
    tasks[2] = (task_t){ TaskComms,   500, 500 };
}

int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();   // remove if you don't need USB

  printf("INA219 + RTOS demo starting...\r\n");

  sensor_read = sensor_ina219;
  comms_send  = comms_uart;

  HAL_Delay(10);
  I2C_Scan();                 // Find devices on the bus

  ina219_init(INA219_FAN_ADDR);
  ina219_init(INA219_PHONE_ADDR);

  HAL_Delay(10);

  /* One-shot INA test: helps confirm wiring + configuration */
  uint16_t testP = ina219_read_power_mW(INA219_FAN_ADDR);
  uint16_t raw_bus = 0, raw_shunt = 0;
  HAL_StatusTypeDef st1 = ina219_read_reg(INA219_FAN_ADDR, INA219_REG_BUS, &raw_bus);
  HAL_StatusTypeDef st2 = ina219_read_reg(INA219_FAN_ADDR, INA219_REG_SHUNT, &raw_shunt);

  printf("Single-shot INA test: st_bus=%d st_shunt=%d bus=0x%04X shunt=0x%04X P=%u mW\r\n",
         st1, st2, raw_bus, raw_shunt, testP);

  const char *start_msg = "RTOS-style 3-task demo start\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)start_msg,
                    strlen(start_msg), HAL_MAX_DELAY);

  init_tasks();

  /* Infinite loop */
  while (1)
  {
    scheduler();
    HAL_Delay(1);
  }
}

/**
  * System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

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

  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * I2C1 Initialization Function
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing           = 0x00303D5B;   // ~100 kHz @ MSI config
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

/**
  * GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

/**
  * USART2 Initialization Function
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
  * This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(100);
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
