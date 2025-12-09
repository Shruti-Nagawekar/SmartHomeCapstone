/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    json_builder.c
  * @brief   Lightweight JSON builder implementation
  ******************************************************************************
  */

#include "json_builder.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define JSON_BUILDER_MIN_SIZE 32

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Append string to buffer with bounds checking
  * @param  jb: JSON builder handle
  * @param  str: String to append
  * @retval 0 on success, -1 on buffer overflow
  */
static int json_append_string(json_builder_t *jb, const char *str)
{
    uint16_t len = strlen(str);
    if (jb->pos + len >= jb->size) {
        return -1; // Buffer overflow
    }
    memcpy(&jb->buffer[jb->pos], str, len);
    jb->pos += len;
    return 0;
}

/**
  * @brief  Append character to buffer with bounds checking
  * @param  jb: JSON builder handle
  * @param  c: Character to append
  * @retval 0 on success, -1 on buffer overflow
  */
static int json_append_char(json_builder_t *jb, char c)
{
    if (jb->pos + 1 >= jb->size) {
        return -1; // Buffer overflow
    }
    jb->buffer[jb->pos++] = c;
    return 0;
}

/**
  * @brief  Convert integer to string and append
  * @param  jb: JSON builder handle
  * @param  value: Integer value
  * @retval 0 on success, -1 on buffer overflow
  */
static int json_append_int(json_builder_t *jb, int32_t value)
{
    char num_buf[12]; // Enough for -2147483648
    int len = 0;
    int32_t temp = value;
    int negative = 0;

    // Handle negative numbers
    if (temp < 0) {
        negative = 1;
        temp = -temp;
    }

    // Convert to string (reverse order)
    if (temp == 0) {
        num_buf[len++] = '0';
    } else {
        while (temp > 0) {
            num_buf[len++] = '0' + (temp % 10);
            temp /= 10;
        }
    }

    // Check buffer space
    if (jb->pos + len + negative >= jb->size) {
        return -1;
    }

    // Write to buffer (correct order)
    if (negative) {
        jb->buffer[jb->pos++] = '-';
    }
    for (int i = len - 1; i >= 0; i--) {
        jb->buffer[jb->pos++] = num_buf[i];
    }

    return 0;
}

/**
  * @brief  Convert unsigned integer to string and append
  * @param  jb: JSON builder handle
  * @param  value: Unsigned integer value
  * @retval 0 on success, -1 on buffer overflow
  */
static int json_append_uint(json_builder_t *jb, uint32_t value)
{
    char num_buf[11]; // Enough for 4294967295
    int len = 0;
    uint32_t temp = value;

    // Convert to string (reverse order)
    if (temp == 0) {
        num_buf[len++] = '0';
    } else {
        while (temp > 0) {
            num_buf[len++] = '0' + (temp % 10);
            temp /= 10;
        }
    }

    // Check buffer space
    if (jb->pos + len >= jb->size) {
        return -1;
    }

    // Write to buffer (correct order)
    for (int i = len - 1; i >= 0; i--) {
        jb->buffer[jb->pos++] = num_buf[i];
    }

    return 0;
}

/* Exported functions --------------------------------------------------------*/

void json_init(json_builder_t *jb, char *buf, uint16_t buf_size)
{
    jb->buffer = buf;
    jb->size = buf_size;
    jb->pos = 0;
    jb->first = 1;
}

void json_start(json_builder_t *jb)
{
    jb->pos = 0;
    jb->first = 1;
    if (jb->size > 0) {
        jb->buffer[0] = '{';
        jb->pos = 1;
    }
}

int json_add_int(json_builder_t *jb, const char *key, int32_t value)
{
    // Add comma if not first field
    if (!jb->first) {
        if (json_append_char(jb, ',') != 0) return -1;
    }
    jb->first = 0;

    // Add key
    if (json_append_char(jb, '"') != 0) return -1;
    if (json_append_string(jb, key) != 0) return -1;
    if (json_append_char(jb, '"') != 0) return -1;
    if (json_append_char(jb, ':') != 0) return -1;

    // Add value
    if (json_append_int(jb, value) != 0) return -1;

    return 0;
}

int json_add_uint(json_builder_t *jb, const char *key, uint32_t value)
{
    // Add comma if not first field
    if (!jb->first) {
        if (json_append_char(jb, ',') != 0) return -1;
    }
    jb->first = 0;

    // Add key
    if (json_append_char(jb, '"') != 0) return -1;
    if (json_append_string(jb, key) != 0) return -1;
    if (json_append_char(jb, '"') != 0) return -1;
    if (json_append_char(jb, ':') != 0) return -1;

    // Add value
    if (json_append_uint(jb, value) != 0) return -1;

    return 0;
}

int json_add_bool(json_builder_t *jb, const char *key, bool value)
{
    // Add comma if not first field
    if (!jb->first) {
        if (json_append_char(jb, ',') != 0) return -1;
    }
    jb->first = 0;

    // Add key
    if (json_append_char(jb, '"') != 0) return -1;
    if (json_append_string(jb, key) != 0) return -1;
    if (json_append_char(jb, '"') != 0) return -1;
    if (json_append_char(jb, ':') != 0) return -1;

    // Add value
    if (value) {
        if (json_append_string(jb, "true") != 0) return -1;
    } else {
        if (json_append_string(jb, "false") != 0) return -1;
    }

    return 0;
}

void json_end(json_builder_t *jb)
{
    if (jb->pos < jb->size) {
        jb->buffer[jb->pos++] = '}';
        jb->buffer[jb->pos] = '\0'; // Null terminator
    }
}

uint16_t json_get_length(json_builder_t *jb)
{
    return jb->pos;
}

