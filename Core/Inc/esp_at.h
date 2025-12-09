/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    esp_at.h
  * @brief   ESP-AT command interface for ESP32 Wi‑Fi module
  ******************************************************************************
  */

#ifndef ESP_AT_H
#define ESP_AT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

typedef enum {
    ESP_AT_OK = 0,
    ESP_AT_ERROR,
    ESP_AT_TIMEOUT,
    ESP_AT_BUSY
} esp_at_status_t;

typedef enum {
    ESP_STATE_IDLE = 0,
    ESP_STATE_INITIALIZING,
    ESP_STATE_WIFI_CONNECTING,
    ESP_STATE_WIFI_CONNECTED,
    ESP_STATE_TCP_CONNECTING,
    ESP_STATE_TCP_CONNECTED,
    ESP_STATE_ERROR
} esp_state_t;

/* Exported constants --------------------------------------------------------*/
#define ESP_AT_RX_BUFFER_SIZE  512
#define ESP_AT_TX_BUFFER_SIZE  256
#define ESP_AT_RESPONSE_TIMEOUT_MS  5000
#define ESP_AT_WIFI_TIMEOUT_MS      15000

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize ESP-AT module
  * @param  huart: UART handle for ESP32 communication
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_init(UART_HandleTypeDef *huart);

/**
  * @brief  Send AT command and wait for response
  * @param  cmd: AT command string (without \r\n)
  * @param  timeout_ms: Timeout in milliseconds
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_send_cmd(const char *cmd, uint32_t timeout_ms);

/**
  * @brief  Send AT command and check for specific response
  * @param  cmd: AT command string
  * @param  expected_response: Expected response string (e.g., "OK", "ERROR")
  * @param  timeout_ms: Timeout in milliseconds
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_send_cmd_expect(const char *cmd, const char *expected_response, uint32_t timeout_ms);

/**
  * @brief  Test ESP32 connection (AT command)
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_test(void);

/**
  * @brief  Reset ESP32 (AT+RST)
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_reset(void);

/**
  * @brief  Set Wi‑Fi mode to station (AT+CWMODE=1)
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_set_wifi_mode(void);

/**
  * @brief  Connect to Wi‑Fi network
  * @param  ssid: Wi‑Fi SSID
  * @param  password: Wi‑Fi password
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_connect_wifi(const char *ssid, const char *password);

/**
  * @brief  Connect to TCP server
  * @param  server_ip: Server IP address
  * @param  port: Server port
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_connect_tcp(const char *server_ip, uint16_t port);

/**
  * @brief  Send HTTP POST request
  * @param  endpoint: HTTP endpoint (e.g., "/api/energy")
  * @param  json_data: JSON payload
  * @param  json_len: JSON payload length
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_send_http_post(const char *endpoint, const char *json_data, uint16_t json_len);

/**
  * @brief  Close TCP connection
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_close_tcp(void);

/**
  * @brief  Get current ESP32 state
  * @retval esp_state_t
  */
esp_state_t esp_at_get_state(void);

/**
  * @brief  Initialize Wi‑Fi connection (full sequence)
  * @param  ssid: Wi‑Fi SSID
  * @param  password: Wi‑Fi password
  * @retval esp_at_status_t
  */
esp_at_status_t esp_at_init_wifi(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif /* ESP_AT_H */

