# Smart Home Energy Monitoring System - Capstone Project

## Overview

This project implements a **Smart Home Energy Monitoring and Control System** with a custom-built real-time operating system (RTOS) on the STM32F446RE microcontroller. The system monitors energy consumption from two INA219 sensors, applies threshold-based control logic, and transmits telemetry data via Wi‑Fi to a web dashboard.

**Key Features:**
- Custom RTOS scheduler with 3 concurrent tasks
- Real-time sensor sampling at 1 kHz
- INA219 current/voltage sensors (I²C)
- Wi‑Fi communication via ESP32 (ESP-AT protocol)
- JSON telemetry format
- Threshold-based automated control
- Inter-task communication via mailbox pattern

---

## Hardware Requirements

- **MCU:** STM32F446RET6 (NUCLEO-F446RE board)
- **Sensors:** 2x INA219 current/voltage sensors (I²C)
  - INA219_FAN_ADDR: 0x40 (fan/load monitoring)
  - INA219_PHONE_ADDR: 0x41 (phone charger monitoring)
- **Wi‑Fi Module:** ESP32 with ESP-AT firmware
- **Communication:**
  - UART2: Debug output (ST-LINK VCP)
  - USART3: ESP32 Wi‑Fi module (PB10/PB11)
  - I2C1: INA219 sensors (PB6/SCL, PB7/SDA)

---

## Software Architecture

### Custom RTOS Scheduler

Cooperative, time-based scheduler using `HAL_GetTick()` for deterministic task execution.

**Task Schedule:**
| Task        | Period | Frequency | Description                    |
|-------------|--------|-----------|--------------------------------|
| TaskSense   | 1 ms   | 1 kHz     | Samples INA219 sensors         |
| TaskControl | 10 ms  | 100 Hz    | Threshold logic, LED control   |
| TaskComms   | 500 ms | 2 Hz      | Transmits JSON via Wi‑Fi/UART |

### Inter-Task Communication

**Mailbox Pattern (Producer-Consumer):**
- `TaskControl` (producer) writes sensor data and control state
- `TaskComms` (consumer) reads and transmits data
- Synchronization via `full` flag

### Modules

1. **JSON Builder** (`json_builder.c/h`)
   - Lightweight JSON formatting (no external libraries)
   - Format: `{"t":1234,"pA":500,"pB":500,"fan":true}`

2. **ESP-AT Module** (`esp_at.c/h`)
   - ESP-AT command protocol implementation
   - Wi‑Fi connection management
   - HTTP POST transmission

3. **INA219 Driver** (in `main.c`)
   - I²C communication
   - Power calculation in milliwatts
   - Two-channel support

---

## Project Structure

```
demo/
├── Core/
│   ├── Inc/
│   │   ├── esp_at.h              # ESP-AT Wi‑Fi module
│   │   ├── json_builder.h        # JSON builder
│   │   ├── main.h
│   │   └── stm32f4xx_hal_conf.h  # HAL config (I2C enabled)
│   └── Src/
│       ├── esp_at.c              # ESP-AT implementation
│       ├── json_builder.c        # JSON builder implementation
│       ├── main.c                # Main application (RTOS + tasks)
│       └── stm32f4xx_hal_msp.c   # MSP init (I2C1, USART3)
├── demo.ioc                      # STM32CubeMX project file
└── README.md                     # This file
```

---

## Building and Running

### Prerequisites
- STM32CubeIDE 1.19.0 or later
- STM32CubeMX (for hardware configuration)

### Build Steps

1. **Clone the repository:**
   ```bash
   git clone https://github.com/Shruti-Nagawekar/SmartHomeCapstone.git
   cd SmartHomeCapstone
   ```

2. **Open in STM32CubeIDE:**
   - File → Import → Existing Projects into Workspace
   - Select the project directory
   - Click Finish

3. **Build the project:**
   - Project → Build All (or Ctrl+B)

4. **Flash to board:**
   - Run → Debug (or F11)

### Configuration

**Wi‑Fi Settings** (in `main.c`):
```c
#define WIFI_SSID        "YourWiFiSSID"
#define WIFI_PASSWORD    "YourWiFiPassword"
#define SERVER_IP        "192.168.1.100"
#define SERVER_PORT      80
#define HTTP_ENDPOINT    "/api/energy"
```

**Sensor Addresses** (in `main.c`):
```c
#define INA219_FAN_ADDR      (0x40 << 1)
#define INA219_PHONE_ADDR    (0x41 << 1)
```

**Communication Mode:**
- Change `comms_send` in `main.c`:
  - `comms_uart` - Debug output via UART2
  - `comms_esp_at` - Wi‑Fi transmission via ESP32

---

## Serial Output

### Debug Mode (UART2)
```
RTOS-style 3-task demo start
Comms mode: UART2 (debug)
{"t":500,"pA":250,"pB":750,"fan":true}
{"t":1000,"pA":495,"pB":505,"fan":false}
```

### Wi‑Fi Mode (USART3)
- JSON data transmitted via HTTP POST to configured server
- Falls back to UART2 debug on Wi‑Fi errors

---

## Control Logic

**Threshold-Based Fan Control:**
- Monitors power consumption from both sensors
- Activates fan (LED indicator) when `powerA > 600 mW` OR `powerB > 600 mW`
- Threshold configurable in `TaskControl()` function

---

## Testing

### Serial Monitor Settings
- **Baud Rate:** 115200
- **Data:** 8 bits
- **Parity:** None
- **Stop Bits:** 1
- **Flow Control:** None

### Expected Behavior
1. System initializes INA219 sensors on boot
2. TaskSense samples sensors at 1 kHz
3. TaskControl applies threshold logic every 10 ms
4. TaskComms transmits JSON data every 500 ms
5. LED (LD2) indicates fan state

---

## Project Requirements Met

✅ **Custom RTOS:** Student-built scheduler (no external libraries)  
✅ **3 Concurrent Tasks:** TaskSense, TaskControl, TaskComms  
✅ **Inter-task Communication:** Mailbox pattern  
✅ **Sensor Integration:** 2x INA219 sensors via I²C  
✅ **1 kHz Sampling:** TaskSense @ 1 ms period  
✅ **Wireless Communication:** ESP32 Wi‑Fi via ESP-AT  
✅ **Real-time Control:** Threshold-based actuator control  
✅ **Structured Data:** JSON telemetry format  

---

## Future Enhancements

- [ ] Web dashboard frontend
- [ ] Average power calculation
- [ ] Energy accumulation over time
- [ ] Configurable thresholds via web interface
- [ ] Multiple load control
- [ ] Predictive load balancing
- [ ] 24-hour continuous operation validation

---

## License

This project is part of a capstone course requirement.

---

## Contributors

- Shruti Nagawekar
- [Your Team Members]

---

## References

- [STM32F4 HAL Documentation](https://www.st.com/resource/en/user_manual/um1725-description-of-stm32f4-hal-and-lowlayer-drivers-stmicroelectronics.pdf)
- [INA219 Datasheet](https://www.ti.com/lit/ds/symlink/ina219.pdf)
- [ESP-AT Command Set](https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/)

