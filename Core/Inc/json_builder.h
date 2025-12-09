/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    json_builder.h
  * @brief   Lightweight JSON builder for sensor data
  ******************************************************************************
  */

#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
typedef struct {
    char *buffer;      // Output buffer
    uint16_t size;     // Buffer size
    uint16_t pos;      // Current position
    uint8_t first;     // First field flag (for comma handling)
} json_builder_t;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize JSON builder
  * @param  jb: JSON builder handle
  * @param  buf: Output buffer
  * @param  buf_size: Buffer size
  * @retval None
  */
void json_init(json_builder_t *jb, char *buf, uint16_t buf_size);

/**
  * @brief  Start JSON object
  * @param  jb: JSON builder handle
  * @retval None
  */
void json_start(json_builder_t *jb);

/**
  * @brief  Add integer field to JSON
  * @param  jb: JSON builder handle
  * @param  key: Field name
  * @param  value: Integer value
  * @retval 0 on success, -1 on buffer overflow
  */
int json_add_int(json_builder_t *jb, const char *key, int32_t value);

/**
  * @brief  Add unsigned integer field to JSON
  * @param  jb: JSON builder handle
  * @param  key: Field name
  * @param  value: Unsigned integer value
  * @retval 0 on success, -1 on buffer overflow
  */
int json_add_uint(json_builder_t *jb, const char *key, uint32_t value);

/**
  * @brief  Add boolean field to JSON
  * @param  jb: JSON builder handle
  * @param  key: Field name
  * @param  value: Boolean value
  * @retval 0 on success, -1 on buffer overflow
  */
int json_add_bool(json_builder_t *jb, const char *key, bool value);

/**
  * @brief  End JSON object
  * @param  jb: JSON builder handle
  * @retval None
  */
void json_end(json_builder_t *jb);

/**
  * @brief  Get current JSON string length
  * @param  jb: JSON builder handle
  * @retval Length of JSON string (excluding null terminator)
  */
uint16_t json_get_length(json_builder_t *jb);

#ifdef __cplusplus
}
#endif

#endif /* JSON_BUILDER_H */

