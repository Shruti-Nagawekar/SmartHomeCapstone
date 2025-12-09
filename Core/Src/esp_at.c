/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    esp_at.c
  * @brief   ESP-AT command implementation for ESP32 Wi‑Fi module
  ******************************************************************************
  */

#include "esp_at.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define AT_CMD_TERMINATOR "\r\n"
#define RESPONSE_OK       "OK"
#define RESPONSE_ERROR    "ERROR"
#define RESPONSE_PROMPT   "> "

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *esp_huart = NULL;
static char rx_buffer[ESP_AT_RX_BUFFER_SIZE];
static esp_state_t esp_state = ESP_STATE_IDLE;

/* Private function prototypes -----------------------------------------------*/
static esp_at_status_t esp_at_wait_response(const char *expected, uint32_t timeout_ms);
static esp_at_status_t esp_at_send_string(const char *str);
static void esp_at_clear_rx_buffer(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Clear RX buffer
  */
static void esp_at_clear_rx_buffer(void)
{
    memset(rx_buffer, 0, ESP_AT_RX_BUFFER_SIZE);
}

/**
  * @brief  Send string over UART
  * @param  str: String to send
  * @retval esp_at_status_t
  */
static esp_at_status_t esp_at_send_string(const char *str)
{
    if (esp_huart == NULL) {
        return ESP_AT_ERROR;
    }

    uint16_t len = strlen(str);
    HAL_StatusTypeDef status = HAL_UART_Transmit(esp_huart, (uint8_t*)str, len, HAL_MAX_DELAY);
    
    if (status != HAL_OK) {
        return ESP_AT_ERROR;
    }
    return ESP_AT_OK;
}

/**
  * @brief  Wait for response from ESP32
  * @param  expected: Expected response string (NULL to accept any)
  * @param  timeout_ms: Timeout in milliseconds
  * @retval esp_at_status_t
  */
static esp_at_status_t esp_at_wait_response(const char *expected, uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_pos = 0;
    uint8_t rx_byte;
    
    esp_at_clear_rx_buffer();
    
    while ((HAL_GetTick() - start_time) < timeout_ms) {
        // Try to receive a byte (non-blocking)
        if (HAL_UART_Receive(esp_huart, &rx_byte, 1, 10) == HAL_OK) {
            if (rx_pos < (ESP_AT_RX_BUFFER_SIZE - 1)) {
                rx_buffer[rx_pos++] = rx_byte;
                rx_buffer[rx_pos] = '\0';
                
                // Check for expected response
                if (expected != NULL) {
                    if (strstr((char*)rx_buffer, expected) != NULL) {
                        return ESP_AT_OK;
                    }
                    if (strstr((char*)rx_buffer, RESPONSE_ERROR) != NULL) {
                        return ESP_AT_ERROR;
                    }
                } else {
                    // Check for OK or ERROR
                    if (strstr((char*)rx_buffer, RESPONSE_OK) != NULL) {
                        return ESP_AT_OK;
                    }
                    if (strstr((char*)rx_buffer, RESPONSE_ERROR) != NULL) {
                        return ESP_AT_ERROR;
                    }
                }
            }
        }
        HAL_Delay(1); // Small delay to prevent CPU spinning
    }
    
    return ESP_AT_TIMEOUT;
}

/* Exported functions --------------------------------------------------------*/

esp_at_status_t esp_at_init(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return ESP_AT_ERROR;
    }
    
    esp_huart = huart;
    esp_state = ESP_STATE_IDLE;
    esp_at_clear_rx_buffer();
    
    return ESP_AT_OK;
}

esp_at_status_t esp_at_send_cmd(const char *cmd, uint32_t timeout_ms)
{
    if (esp_huart == NULL) {
        return ESP_AT_ERROR;
    }
    
    // Send command
    if (esp_at_send_string(cmd) != ESP_AT_OK) {
        return ESP_AT_ERROR;
    }
    if (esp_at_send_string(AT_CMD_TERMINATOR) != ESP_AT_OK) {
        return ESP_AT_ERROR;
    }
    
    // Wait for response
    return esp_at_wait_response(NULL, timeout_ms);
}

esp_at_status_t esp_at_send_cmd_expect(const char *cmd, const char *expected_response, uint32_t timeout_ms)
{
    if (esp_huart == NULL) {
        return ESP_AT_ERROR;
    }
    
    // Send command
    if (esp_at_send_string(cmd) != ESP_AT_OK) {
        return ESP_AT_ERROR;
    }
    if (esp_at_send_string(AT_CMD_TERMINATOR) != ESP_AT_OK) {
        return ESP_AT_ERROR;
    }
    
    // Wait for specific response
    return esp_at_wait_response(expected_response, timeout_ms);
}

esp_at_status_t esp_at_test(void)
{
    esp_at_status_t status = esp_at_send_cmd("AT", ESP_AT_RESPONSE_TIMEOUT_MS);
    if (status == ESP_AT_OK) {
        esp_state = ESP_STATE_IDLE;
    }
    return status;
}

esp_at_status_t esp_at_reset(void)
{
    esp_state = ESP_STATE_INITIALIZING;
    esp_at_status_t status = esp_at_send_cmd("AT+RST", 10000); // Reset takes longer
    HAL_Delay(2000); // Give ESP32 time to boot after reset
    
    if (status == ESP_AT_OK || status == ESP_AT_TIMEOUT) {
        // After reset, ESP32 may not respond immediately, so test again
        HAL_Delay(1000);
        status = esp_at_test();
    }
    
    return status;
}

esp_at_status_t esp_at_set_wifi_mode(void)
{
    return esp_at_send_cmd_expect("AT+CWMODE=1", RESPONSE_OK, ESP_AT_RESPONSE_TIMEOUT_MS);
}

esp_at_status_t esp_at_connect_wifi(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_AT_ERROR;
    }
    
    esp_state = ESP_STATE_WIFI_CONNECTING;
    
    // Build command: AT+CWJAP="SSID","PASSWORD"
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    
    esp_at_status_t status = esp_at_send_cmd_expect(cmd, RESPONSE_OK, ESP_AT_WIFI_TIMEOUT_MS);
    
    if (status == ESP_AT_OK) {
        esp_state = ESP_STATE_WIFI_CONNECTED;
    } else {
        esp_state = ESP_STATE_ERROR;
    }
    
    return status;
}

esp_at_status_t esp_at_connect_tcp(const char *server_ip, uint16_t port)
{
    if (server_ip == NULL) {
        return ESP_AT_ERROR;
    }
    
    esp_state = ESP_STATE_TCP_CONNECTING;
    
    // Build command: AT+CIPSTART="TCP","IP",PORT
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", server_ip, port);
    
    esp_at_status_t status = esp_at_send_cmd_expect(cmd, RESPONSE_OK, ESP_AT_RESPONSE_TIMEOUT_MS);
    
    if (status == ESP_AT_OK) {
        esp_state = ESP_STATE_TCP_CONNECTED;
    } else {
        esp_state = ESP_STATE_ERROR;
    }
    
    return status;
}

esp_at_status_t esp_at_send_http_post(const char *endpoint, const char *json_data, uint16_t json_len)
{
    if (endpoint == NULL || json_data == NULL || json_len == 0) {
        return ESP_AT_ERROR;
    }
    
    // Build HTTP POST request
    // Note: Host header should contain server IP or domain
    // For simplicity, we'll use a placeholder that the server can handle
    char http_request[512];
    int http_len = snprintf(http_request, sizeof(http_request),
        "POST %s HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        endpoint, json_len, json_data);
    
    if (http_len < 0 || http_len >= (int)sizeof(http_request)) {
        return ESP_AT_ERROR;
    }
    
    // Send AT+CIPSEND command
    char cipsend_cmd[32];
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d", http_len);
    
    esp_at_status_t status = esp_at_send_cmd_expect(cipsend_cmd, RESPONSE_PROMPT, ESP_AT_RESPONSE_TIMEOUT_MS);
    if (status != ESP_AT_OK) {
        return status;
    }
    
    // Send HTTP request data
    status = esp_at_send_string(http_request);
    if (status != ESP_AT_OK) {
        return status;
    }
    
    // Wait for response (SEND OK)
    return esp_at_wait_response(RESPONSE_OK, ESP_AT_RESPONSE_TIMEOUT_MS);
}

esp_at_status_t esp_at_close_tcp(void)
{
    esp_at_status_t status = esp_at_send_cmd("AT+CIPCLOSE", ESP_AT_RESPONSE_TIMEOUT_MS);
    if (status == ESP_AT_OK) {
        esp_state = ESP_STATE_WIFI_CONNECTED;
    }
    return status;
}

esp_state_t esp_at_get_state(void)
{
    return esp_state;
}

esp_at_status_t esp_at_init_wifi(const char *ssid, const char *password)
{
    // Full initialization sequence
    esp_at_status_t status;
    
    // 1. Test connection
    status = esp_at_test();
    if (status != ESP_AT_OK) {
        return status;
    }
    
    // 2. Set Wi‑Fi mode
    status = esp_at_set_wifi_mode();
    if (status != ESP_AT_OK) {
        return status;
    }
    
    // 3. Connect to Wi‑Fi
    status = esp_at_connect_wifi(ssid, password);
    if (status != ESP_AT_OK) {
        return status;
    }
    
    return ESP_AT_OK;
}

