#include "main.h"
#include "json_builder.h"
#include "esp_at.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Function prototypes from CubeMX */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);

/* UART handles */
UART_HandleTypeDef huart2;  // UART2: Debug (ST-LINK VCP)
UART_HandleTypeDef huart3;  // USART3: ESP32 Wi‑Fi module
I2C_HandleTypeDef  hi2c1;   // I2C handle for INA219

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

/* ===================== Sensor Abstraction ===================== */

typedef void (*sensor_read_fn_t)(uint16_t *powerA, uint16_t *powerB);

static volatile uint16_t powerA = 0;
static volatile uint16_t powerB = 0;

/* Simulated sensor (for testing without hardware) */
static void sensor_simulated(uint16_t *pA, uint16_t *pB)
{
    static uint16_t x = 0;
    x = (x + 5) % 1000;    // ramp 0..995 in steps of 5
    *pA = x;
    *pB = 1000 - x;
}

/* Real INA219 sensor implementation */
static void sensor_ina219(uint16_t *pA, uint16_t *pB)
{
    uint16_t p_fan_mW   = ina219_read_power_mW(INA219_FAN_ADDR);
    uint16_t p_phone_mW = ina219_read_power_mW(INA219_PHONE_ADDR);

    *pA = p_fan_mW;
    *pB = p_phone_mW;
}

/* Select sensor: sensor_ina219 (real) or sensor_simulated (test) */
static sensor_read_fn_t sensor_read = sensor_ina219;

/* ===================== Wi‑Fi Configuration ===================== */
/* TODO: Configure these values for your network */
#define WIFI_SSID        "YourWiFiSSID"
#define WIFI_PASSWORD    "YourWiFiPassword"
#define SERVER_IP        "192.168.1.100"  // Web server IP address
#define SERVER_PORT      80              // HTTP port (80 or 8080)
#define HTTP_ENDPOINT    "/api/energy"

/* ===================== Comms Abstraction ===================== */

typedef void (*comms_send_fn_t)(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan);

/* JSON buffer for telemetry */
#define JSON_BUFFER_SIZE 256
static char json_buffer[JSON_BUFFER_SIZE];
static json_builder_t json_builder;

/* Default: JSON output over UART2 (debug) */
static void comms_uart(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan)
{
    // Build JSON message
    json_start(&json_builder);
    json_add_uint(&json_builder, "t", ticks);
    json_add_uint(&json_builder, "pA", pA);
    json_add_uint(&json_builder, "pB", pB);
    json_add_bool(&json_builder, "fan", (fan != 0));
    json_end(&json_builder);

    // Send JSON over UART2 (debug)
    uint16_t len = json_get_length(&json_builder);
    HAL_UART_Transmit(&huart2, (uint8_t*)json_buffer, len, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}

/* ESP-AT Wi‑Fi JSON telemetry */
static uint8_t esp_at_initialized = 0;
static uint8_t esp_at_wifi_connected = 0;
static uint8_t esp_at_tcp_connected = 0;

static void comms_esp_at(uint32_t ticks, uint16_t pA, uint16_t pB, uint8_t fan)
{
    esp_at_status_t status;
    
    // Initialize ESP-AT if not done
    if (!esp_at_initialized) {
        status = esp_at_init(&huart3);
        if (status != ESP_AT_OK) {
            // Fallback to debug UART on error
            comms_uart(ticks, pA, pB, fan);
            return;
        }
        
        // Initialize Wi‑Fi connection
        status = esp_at_init_wifi(WIFI_SSID, WIFI_PASSWORD);
        if (status != ESP_AT_OK) {
            // Fallback to debug UART on error
            comms_uart(ticks, pA, pB, fan);
            return;
        }
        
        esp_at_initialized = 1;
        esp_at_wifi_connected = 1;
    }
    
    // Connect TCP if not connected
    if (!esp_at_tcp_connected) {
        status = esp_at_connect_tcp(SERVER_IP, SERVER_PORT);
        if (status != ESP_AT_OK) {
            // Fallback to debug UART on error
            comms_uart(ticks, pA, pB, fan);
            return;
        }
        esp_at_tcp_connected = 1;
    }
    
    // Build JSON message
    json_start(&json_builder);
    json_add_uint(&json_builder, "t", ticks);
    json_add_uint(&json_builder, "pA", pA);
    json_add_uint(&json_builder, "pB", pB);
    json_add_bool(&json_builder, "fan", (fan != 0));
    json_end(&json_builder);
    
    // Send HTTP POST with JSON
    uint16_t json_len = json_get_length(&json_builder);
    status = esp_at_send_http_post(HTTP_ENDPOINT, json_buffer, json_len);
    
    if (status != ESP_AT_OK) {
        // On error, close TCP and try to reconnect next time
        esp_at_close_tcp();
        esp_at_tcp_connected = 0;
        // Fallback to debug UART
        comms_uart(ticks, pA, pB, fan);
    }
}

/* Select communication method: comms_uart (debug) or comms_esp_at (Wi‑Fi) */
static comms_send_fn_t comms_send = comms_uart;  // Change to comms_esp_at to enable Wi‑Fi

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
    MX_USART2_UART_Init();   // UART2 on STLink VCP (debug)
    MX_USART3_UART_Init();   // USART3 for ESP32 Wi‑Fi module
    MX_I2C1_Init();          // I2C for INA219 sensors

    /* Initialize INA219 sensors */
    ina219_init(INA219_FAN_ADDR);       // Initialize the fan sensor
    ina219_init(INA219_PHONE_ADDR);      // Initialize the phone charger sensor

    const char *start_msg = "RTOS-style 3-task demo start\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)start_msg,
                      strlen(start_msg), HAL_MAX_DELAY);

    /* Initialize JSON builder */
    json_init(&json_builder, json_buffer, JSON_BUFFER_SIZE);

    /* Print communication mode */
    if (comms_send == comms_uart) {
        printf("Comms mode: UART2 (debug)\r\n");
    } else {
        printf("Comms mode: ESP32 Wi‑Fi (USART3)\r\n");
        printf("Wi‑Fi SSID: %s\r\n", WIFI_SSID);
        printf("Server: %s:%u%s\r\n", SERVER_IP, SERVER_PORT, HTTP_ENDPOINT);
    }

    /* Initialize the software RTOS scheduler */
    init_tasks();

    while (1)
    {
        scheduler();     // run scheduler every tick
        HAL_Delay(1);    // 1 ms granularity
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

static void MX_USART3_UART_Init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK) {
        Error_Handler();
    }
}

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

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
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
