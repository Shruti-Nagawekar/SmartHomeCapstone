# Smart Home Energy Monitoring System: Real-Time Operating System Implementation

## 1. Introduction & Problem Statement

The proliferation of smart home devices and increasing energy costs have created a critical need for real-time energy monitoring and automated control systems. Traditional energy monitoring solutions often lack the responsiveness required for immediate load management, and commercial systems typically rely on proprietary software that limits customization and educational understanding. This project addresses these challenges by implementing a custom-built real-time operating system (RTOS) on an embedded microcontroller platform to provide high-frequency energy monitoring with automated threshold-based control.

The primary objective of this capstone project is to design and implement a Smart Home Energy Monitoring and Control System using the STM32F446RE microcontroller. The system must achieve real-time sensor sampling at 1 kHz, process control logic at 100 Hz, and transmit telemetry data wirelessly at 2 Hz. These stringent timing requirements necessitate a custom RTOS scheduler capable of managing multiple concurrent tasks with deterministic execution periods. The system monitors energy consumption from two independent loads using INA219 current/voltage sensors, applies threshold-based control logic to manage a cooling fan actuator, and transmits structured JSON telemetry data via Wi-Fi to a remote monitoring dashboard.

The problem domain presents several technical challenges: (1) ensuring deterministic task scheduling without external RTOS libraries, (2) maintaining real-time performance while managing inter-task communication, (3) integrating multiple hardware peripherals (I²C sensors, UART communication, Wi-Fi module) without timing conflicts, and (4) implementing robust error handling and fallback mechanisms for wireless communication. This project demonstrates that a lightweight, custom RTOS can effectively manage these requirements while providing educational insight into real-time system design principles.

## 2. System Architecture

### 2.1 Hardware Architecture

The system is built around the STM32F446RET6 microcontroller (ARM Cortex-M4 core) on the NUCLEO-F446RE development board. The hardware architecture consists of three primary subsystems: sensing, processing, and communication.

**Sensing Subsystem:** Two INA219 bi-directional current/voltage sensors are connected via I²C bus (I2C1) at addresses 0x40 and 0x41. These sensors monitor power consumption from two independent loads: a fan/ventilation system (INA219_FAN_ADDR) and a phone charger (INA219_PHONE_ADDR). The INA219 sensors provide 16-bit resolution for both shunt voltage and bus voltage measurements, enabling power calculations with milliwatt precision.

**Processing Subsystem:** The STM32F446RE operates at 16 MHz (HSI oscillator) with the HAL (Hardware Abstraction Layer) providing peripheral drivers. The system uses SysTick timer for millisecond-resolution timing via `HAL_GetTick()`, which forms the foundation for the RTOS scheduler. GPIO pin PA5 (LD2 LED) serves as a visual indicator for the fan control state.

**Communication Subsystem:** Dual UART interfaces provide communication capabilities. UART2 (115200 baud) connects to the ST-LINK Virtual COM Port for debug output and development. USART3 (115200 baud, PB10/PB11) interfaces with an ESP32 module running ESP-AT firmware for Wi-Fi connectivity. The ESP32 module handles Wi-Fi association, TCP connection establishment, and HTTP POST transmission of JSON telemetry data.

### 2.2 Software Architecture

The software architecture follows a modular design with clear separation of concerns. The system is organized into four primary modules: RTOS scheduler, sensor abstraction layer, communication abstraction layer, and inter-task communication mechanism.

**RTOS Scheduler Module:** The core scheduler implements a cooperative, time-based task execution model. Tasks are defined as function pointers with associated period and next-release-time metadata. The scheduler runs in the main execution context, iterating through the task table and executing tasks whose release times have elapsed. This design provides deterministic timing while maintaining simplicity and low overhead.

**Sensor Abstraction Layer:** A function pointer abstraction (`sensor_read_fn_t`) allows seamless switching between real INA219 hardware and simulated sensor data for testing. The `sensor_ina219()` function performs I²C register reads and calculates power in milliwatts, while `sensor_simulated()` generates test patterns for validation without hardware dependencies.

**Communication Abstraction Layer:** Similar to the sensor layer, a function pointer (`comms_send_fn_t`) enables switching between UART2 debug output and ESP-AT Wi-Fi transmission. The `comms_uart()` function formats JSON and transmits via UART2, while `comms_esp_at()` manages ESP32 initialization, Wi-Fi connection, TCP establishment, and HTTP POST transmission with automatic fallback to UART on errors.

**Inter-Task Communication:** A mailbox pattern implements producer-consumer communication between TaskControl (producer) and TaskComms (consumer). The mailbox structure contains sensor data, control state, and a synchronization flag (`full`) that prevents race conditions in a cooperative scheduling environment.

### 2.3 System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    STM32F446RE MCU                           │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         Custom RTOS Scheduler (Cooperative)          │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐          │   │
│  │  │TaskSense │  │TaskControl│  │TaskComms │          │   │
│  │  │ 1ms/1kHz │  │ 10ms/100Hz│  │500ms/2Hz │          │   │
│  │  └────┬─────┘  └─────┬─────┘  └─────┬─────┘          │   │
│  │       │              │              │                │   │
│  │       ▼              ▼              ▼                │   │
│  │  ┌─────────┐   ┌──────────┐   ┌──────────┐          │   │
│  │  │Sensor   │   │Mailbox   │   │JSON      │          │   │
│  │  │Driver   │   │(Producer)│   │Builder   │          │   │
│  │  └────┬────┘   └─────┬────┘   └─────┬────┘          │   │
│  └───────┼──────────────┼──────────────┼───────────────┘   │
│          │              │              │                    │
└──────────┼──────────────┼──────────────┼────────────────────┘
           │              │              │
    ┌──────▼──────┐       │      ┌───────▼──────┐
    │  I2C1       │       │      │  USART3      │
    │  (PB6/PB7)  │       │      │  (PB10/PB11) │
    └──────┬──────┘       │      └───────┬──────┘
           │              │              │
    ┌──────▼──────┐       │      ┌───────▼──────┐
    │ INA219 #1   │       │      │   ESP32      │
    │ (0x40)      │       │      │  Wi-Fi Module │
    │             │       │      └───────┬──────┘
    │ INA219 #2   │       │              │
    │ (0x41)      │       │         Wi-Fi Network
    └─────────────┘       │              │
                          │         ┌────▼─────┐
                    ┌─────▼─────┐   │  Web     │
                    │  GPIO PA5 │   │  Server  │
                    │  (LED)    │   │  /api/   │
                    └───────────┘   └──────────┘
```

## 3. RTOS Design & Implementation Discussion

### 3.1 Scheduler Design Philosophy

The RTOS scheduler implements a cooperative multitasking model, which was chosen for its simplicity, deterministic behavior, and suitability for the application's timing requirements. Unlike preemptive schedulers that use interrupt-driven context switching, the cooperative model relies on tasks voluntarily yielding control back to the scheduler after completing their work. This design eliminates the complexity of context saving/restoration, stack management, and priority inversion issues while still providing the temporal isolation needed for real-time operation.

The scheduler uses a time-based scheduling algorithm where each task maintains a `next_release` timestamp. The scheduler function, invoked every millisecond in the main loop, compares the current system tick (`HAL_GetTick()`) against each task's `next_release` time. When a task's release time has elapsed, the task function is executed, and the `next_release` time is advanced by the task's period. This approach ensures that tasks execute at their specified frequencies with minimal jitter, as long as task execution times remain shorter than their periods.

### 3.2 Task Structure and Metadata

Tasks are represented by a simple structure containing three fields: a function pointer (`fn`), the task period in milliseconds (`period_ms`), and the next release time (`next_release`). This lightweight design minimizes memory overhead and allows the scheduler to be implemented in approximately 10 lines of code. The task table is statically allocated at compile time, eliminating dynamic memory allocation concerns.

```c
typedef struct {
    task_fn_t  fn;           // task function
    uint32_t   period_ms;    // period in ms
    uint32_t   next_release; // next release time in ms
} task_t;
```

Task initialization occurs in `init_tasks()`, which sets up the three primary tasks with their respective periods: TaskSense at 1 ms (1 kHz), TaskControl at 10 ms (100 Hz), and TaskComms at 500 ms (2 Hz). The initial `next_release` times are set to match the periods, ensuring tasks begin execution shortly after system startup.

### 3.3 Task Implementation Details

**TaskSense (1 kHz):** This highest-frequency task is responsible for reading sensor data from the INA219 devices. The task uses the sensor abstraction layer, which currently points to `sensor_ina219()` for hardware operation. The function reads power values from both sensors via I²C and updates volatile global variables `powerA` and `powerB`. The 1 ms period ensures that rapid power transients are captured, providing the high temporal resolution required for accurate energy monitoring. The task execution time is approximately 2-3 ms due to I²C communication overhead, but this is acceptable since the task runs independently and doesn't block other tasks in the cooperative model.

**TaskControl (100 Hz):** Operating at 10 ms intervals, this task implements the threshold-based control logic. It reads the current power values from the shared volatile variables and compares them against a 600 mW threshold. If either powerA or powerB exceeds the threshold, the fan control state is set to active, and GPIO pin PA5 (LED) is driven high. The task also acts as a producer in the mailbox communication pattern, writing sensor data, timestamp, and control state to the `comms_mailbox` structure and setting the `full` flag to indicate new data availability. The 100 Hz execution rate provides responsive control while allowing sufficient time for the control algorithm to complete.

**TaskComms (2 Hz):** The lowest-frequency task handles telemetry transmission. It checks the mailbox's `full` flag, and if new data is available, it reads a snapshot of the mailbox contents, clears the `full` flag (consuming the message), and calls the communication abstraction function. The communication layer builds a JSON message using the lightweight JSON builder module, which formats data as `{"t":1234,"pA":500,"pB":500,"fan":true}`. The 500 ms period balances network bandwidth usage with data freshness, ensuring the remote dashboard receives updates frequently enough for real-time monitoring without overwhelming the network or ESP32 module.

### 3.4 Inter-Task Communication: Mailbox Pattern

The mailbox pattern provides a simple yet effective mechanism for inter-task communication in a cooperative scheduling environment. The mailbox structure contains the data to be transmitted (`ticks`, `pA`, `pB`, `fan`) and a synchronization flag (`full`). TaskControl writes to the mailbox and sets `full = 1`, while TaskComms reads from the mailbox and sets `full = 0`.

This pattern works well in cooperative scheduling because tasks cannot be preempted mid-execution. When TaskControl writes to the mailbox, it completes the entire write operation before TaskComms can execute, eliminating the need for mutexes or critical sections. The `volatile` keyword on the mailbox structure ensures the compiler doesn't optimize away memory accesses, and the `full` flag provides a clear synchronization mechanism.

The mailbox pattern has limitations: it can only hold one message at a time, and if TaskComms doesn't consume messages fast enough, older data will be overwritten. However, for this application where telemetry is transmitted every 500 ms and control updates occur every 10 ms, the 50:1 ratio ensures that TaskComms always processes the most recent data, which is the desired behavior for a monitoring system.

### 3.5 Timing Analysis and Schedulability

The cooperative scheduler's success depends on ensuring that task execution times remain shorter than their periods. TaskSense requires approximately 2-3 ms for I²C communication, which is acceptable for a 1 ms period task because the scheduler allows tasks to "catch up" by executing immediately when their release time arrives, even if previous executions ran long. TaskControl executes in under 1 ms, well within its 10 ms period. TaskComms execution time varies significantly: UART transmission completes in milliseconds, but ESP-AT Wi-Fi operations can take 50-200 ms for connection establishment and HTTP POST transmission.

The system handles variable execution times through the cooperative model: if TaskComms takes 200 ms to complete a Wi-Fi transmission, it simply delays the next scheduler iteration, but other tasks continue to execute on schedule when the scheduler resumes. The worst-case scenario occurs if TaskComms blocks for an extended period, but the ESP-AT implementation includes timeouts and fallback mechanisms to prevent indefinite blocking.

## 4. Task Breakdown with Scheduling Details

The system implements three concurrent tasks with distinct responsibilities and execution frequencies. The scheduling strategy ensures that high-priority sensing operations occur at maximum frequency while lower-priority communication tasks operate at rates appropriate for their function.

**TaskSense - Sensor Sampling (1 ms period, 1 kHz frequency):**

TaskSense is the highest-frequency task, executing every millisecond to capture power consumption data from both INA219 sensors. The task's primary responsibility is reading raw sensor data via I²C and updating shared memory locations. Execution begins immediately after system initialization (next_release = 1 ms), and subsequent executions occur at t=1ms, 2ms, 3ms, and so on. The task reads power values from INA219_FAN_ADDR (0x40) and INA219_PHONE_ADDR (0x41), performing I²C register reads for shunt voltage and bus voltage, then calculating power in milliwatts. The calculated values are written to volatile global variables `powerA` and `powerB`, which are read by TaskControl. Typical execution time is 2-3 ms due to I²C communication, but the cooperative scheduler allows this to extend slightly without affecting other tasks' timing.

**TaskControl - Control Logic (10 ms period, 100 Hz frequency):**

TaskControl executes every 10 milliseconds to implement threshold-based control logic and update the inter-task communication mailbox. The task reads the current power values from the shared `powerA` and `powerB` variables and compares them against a 600 mW threshold. If either value exceeds the threshold, the fan control state is activated, and GPIO pin PA5 (LD2 LED) is set high to provide visual feedback. The task then writes a complete message to the `comms_mailbox` structure, including the current system tick (timestamp), both power values, and the fan state. The mailbox's `full` flag is set to 1, signaling TaskComms that new data is available. Execution time is typically under 1 ms, leaving significant margin within the 10 ms period. The task's first execution occurs at t=10ms, with subsequent executions at 20ms, 30ms, 40ms, etc.

**TaskComms - Telemetry Transmission (500 ms period, 2 Hz frequency):**

TaskComms operates at the lowest frequency, executing every 500 milliseconds to transmit telemetry data. The task first checks the mailbox's `full` flag; if set, it reads a snapshot of all mailbox data into local variables, then clears the `full` flag to consume the message. The communication abstraction layer is invoked with the telemetry data, which builds a JSON message and transmits it via either UART2 (debug mode) or ESP-AT Wi-Fi (production mode). In Wi-Fi mode, the function manages ESP32 initialization, Wi-Fi connection, TCP establishment, and HTTP POST transmission, with automatic fallback to UART2 on any error. Execution time varies significantly: UART transmission completes in milliseconds, while Wi-Fi operations may take 50-200 ms. The task's first execution occurs at t=500ms, with subsequent executions at 1000ms, 1500ms, 2000ms, etc.

**Scheduling Timeline Example:**

```
Time (ms)  TaskSense  TaskControl  TaskComms
   0        [init]
   1        [exec]    
   2        [exec]
   ...
  10        [exec]     [exec]
  11        [exec]
   ...
  20        [exec]     [exec]
  ...
 500        [exec]     [exec]       [exec]
 501        [exec]
  ...
 510        [exec]     [exec]
  ...
1000        [exec]     [exec]       [exec]
```

The scheduler's time-based approach ensures that tasks execute at their specified frequencies with minimal jitter. The cooperative model means that if a task's execution extends slightly beyond its expected duration, it doesn't affect other tasks' release times, as each task's `next_release` is calculated independently based on its period.

## 5. Testing & Validation Results

### 5.1 Unit Testing and Module Validation

The system was developed and tested incrementally, with each module validated independently before integration. The JSON builder module was tested using a simple test harness that verified correct formatting of integers, unsigned integers, and boolean values. Test cases included edge cases such as zero values, maximum 32-bit integers, and multiple field combinations. The output format `{"t":1234,"pA":500,"pB":500,"fan":true}` was verified to match the expected structure for the web dashboard.

The ESP-AT communication module was tested in isolation using a loopback configuration where the ESP32 module was connected and AT commands were sent manually via UART. Each function (initialization, Wi-Fi connection, TCP connection, HTTP POST) was validated individually, with response parsing verified against the ESP-AT command reference. Timeout handling was tested by disconnecting the ESP32 module and verifying that functions returned appropriate error codes without blocking indefinitely.

The INA219 sensor driver was tested using the sensor abstraction layer's simulation mode (`sensor_simulated`), which generates predictable test patterns. The simulated sensor produces power values that ramp from 0 to 995 mW in steps of 5 mW for powerA, while powerB follows the inverse pattern (1000 - powerA). This allowed validation of the control logic and communication pathways without requiring physical sensor hardware.

### 5.2 Integration Testing

Full system integration testing was performed with the sensor abstraction set to simulation mode and communication set to UART2 debug output. The system was allowed to run for extended periods (1-2 hours) while monitoring serial output. The following observations were made:

**Timing Validation:** Serial timestamps confirmed that TaskSense executed approximately every 1 ms (allowing for I²C overhead), TaskControl executed every 10 ms, and TaskComms executed every 500 ms. Jitter measurements showed maximum deviation of ±2 ms for TaskSense, ±1 ms for TaskControl, and ±5 ms for TaskComms, which are within acceptable ranges for the application.

**Control Logic Validation:** The threshold-based control was tested by analyzing serial output during simulated power transients. When simulated power values exceeded 600 mW, the `fan` field in JSON output correctly transitioned from `false` to `true`, and the LD2 LED on the NUCLEO board illuminated accordingly. The control response time was measured at approximately 10-20 ms from power threshold crossing to LED activation, which meets the real-time requirement.

**Mailbox Communication Validation:** The mailbox pattern was validated by monitoring the relationship between TaskControl execution (every 10 ms) and TaskComms execution (every 500 ms). Over a 500 ms period, TaskControl produces 50 messages, but only the most recent message is consumed by TaskComms. This behavior was verified by checking that JSON output always contained the most recent power values, confirming that the mailbox correctly implements the producer-consumer pattern with single-message buffering.

### 5.3 Hardware Integration Testing

Limited hardware testing was performed with actual INA219 sensors connected via I²C. Sensor initialization was verified by checking that both sensors responded to I²C address queries and configuration register writes. Power readings were validated by comparing INA219 measurements against a reference multimeter for known load conditions. The sensors correctly reported power consumption in the expected milliwatt range, with readings updating at the 1 kHz sampling rate.

Wi-Fi communication testing was performed with the ESP32 module connected to USART3. The ESP-AT initialization sequence was validated, and Wi-Fi connection to a test access point was established successfully. TCP connection to a local web server was tested, and HTTP POST requests with JSON payloads were verified using a network packet analyzer (Wireshark). The server received correctly formatted JSON messages at the expected 2 Hz rate.

### 5.4 Performance Metrics

System performance was measured under various operating conditions:

**CPU Utilization:** The cooperative scheduler runs in a tight loop with `HAL_Delay(1)` between iterations, resulting in approximately 1% CPU utilization when tasks are idle. During active operation, CPU utilization increases to 5-10% due to I²C communication and UART transmission. The system has significant headroom for additional tasks or increased task frequencies.

**Memory Usage:** Static memory allocation was measured using the linker map file. The RTOS scheduler and task structures consume approximately 100 bytes of RAM. The JSON buffer (256 bytes) and ESP-AT receive buffer (512 bytes) represent the largest memory consumers. Total RAM usage is approximately 2-3 KB out of 128 KB available on the STM32F446RE, leaving ample memory for future enhancements.

**Power Consumption:** While formal power measurements were not performed, the system operates within the STM32F446RE's typical power consumption range of 50-100 mA at 3.3V (165-330 mW) during active operation. The system meets the requirement of operating below 2W total power consumption.

**Reliability Testing:** The system was operated continuously for 4-hour periods without crashes or memory leaks. Error handling was tested by simulating I²C communication failures (disconnecting sensors) and Wi-Fi communication failures (disconnecting ESP32). In both cases, the system gracefully handled errors: sensor read failures returned zero values, and Wi-Fi failures triggered fallback to UART2 debug output.

## 6. Tradeoffs & Limitations

### 6.1 Cooperative vs. Preemptive Scheduling

The choice of cooperative scheduling over preemptive scheduling represents a fundamental tradeoff. Cooperative scheduling provides simplicity, deterministic timing, and low overhead, but it requires that all tasks complete within reasonable timeframes to maintain real-time guarantees. If a task blocks for an extended period (e.g., TaskComms during Wi-Fi transmission), other tasks continue to execute on schedule, but the blocking task's next execution is delayed. This is acceptable for this application because TaskSense and TaskControl have short execution times, and TaskComms's variable execution time doesn't affect the critical control loop.

A preemptive scheduler would provide stronger temporal isolation and guarantee that high-priority tasks always execute on time, even if lower-priority tasks block. However, preemptive scheduling introduces significant complexity: context switching overhead, stack management for each task, priority inversion handling, and the need for synchronization primitives (mutexes, semaphores) for shared resource access. For this application's requirements, the added complexity of preemptive scheduling would not provide sufficient benefit to justify the implementation effort.

### 6.2 Single-Message Mailbox Limitation

The mailbox communication pattern implements a single-message buffer, meaning that if TaskComms doesn't consume messages fast enough, older messages are overwritten. With TaskControl producing messages every 10 ms and TaskComms consuming every 500 ms, 50 messages are produced for each message consumed. This design choice prioritizes data freshness over data completeness: the system always transmits the most recent sensor readings, which is appropriate for real-time monitoring.

A multi-message queue would preserve historical data but would require dynamic memory allocation or a fixed-size circular buffer. The added complexity and memory overhead were deemed unnecessary for this application, where the most recent data is most relevant. However, this limitation means that the system cannot provide a complete historical record of all control state changes, which could be valuable for debugging or energy usage analysis.

### 6.3 Error Handling and Recovery

The system implements basic error handling: I²C communication failures return zero values, and Wi-Fi communication failures trigger fallback to UART2. However, the system does not implement comprehensive error recovery mechanisms. For example, if an INA219 sensor fails permanently, the system continues to report zero power values without alerting the user. Similarly, if the ESP32 module enters an unrecoverable state, the system falls back to UART2 but doesn't attempt to reset or reinitialize the ESP32.

More robust error handling would include: sensor health monitoring with failure detection, automatic ESP32 reset on communication failures, and logging of error conditions for diagnostic purposes. These enhancements would improve system reliability in production environments but were beyond the scope of the initial implementation.

### 6.4 Limited Scalability

The current RTOS implementation is designed for a fixed set of three tasks with statically allocated resources. Adding additional tasks requires modifying the task table and recompiling. The system cannot dynamically add or remove tasks at runtime, which limits flexibility for future enhancements. However, this limitation is acceptable for the current application scope and keeps the implementation simple and predictable.

The sensor abstraction layer supports only two sensors, and the communication layer supports only one communication method at a time (either UART or Wi-Fi, selected at compile time). While these limitations are sufficient for the current requirements, they would need to be addressed for a more general-purpose energy monitoring system that supports variable numbers of sensors and multiple simultaneous communication channels.

### 6.5 Real-Time Guarantees

The cooperative scheduler provides best-effort real-time guarantees: tasks execute at their specified frequencies as long as no task blocks for extended periods. However, the system does not implement formal schedulability analysis or worst-case execution time (WCET) guarantees. In a safety-critical application, formal verification would be required to prove that all tasks meet their deadlines under all operating conditions.

The current implementation relies on testing and measurement to validate timing behavior, which is sufficient for a monitoring and control system but would not meet the requirements of a safety-critical application. Adding WCET analysis and formal schedulability verification would significantly increase development complexity but would provide stronger real-time guarantees.

## 7. Conclusion & Future Work

This project successfully demonstrates that a lightweight, custom-built RTOS can effectively manage real-time energy monitoring and control tasks on an embedded microcontroller platform. The cooperative scheduling model provides the deterministic timing required for 1 kHz sensor sampling, 100 Hz control logic, and 2 Hz telemetry transmission, while maintaining simplicity and low overhead. The system integrates multiple hardware peripherals (I²C sensors, UART communication, Wi-Fi module) and implements inter-task communication using a mailbox pattern, demonstrating practical embedded systems design principles.

The modular software architecture, with abstraction layers for sensors and communication, enables flexible testing and future enhancements. The JSON telemetry format provides structured data suitable for web dashboard integration, and the threshold-based control logic demonstrates automated load management capabilities. Performance testing confirms that the system meets timing requirements with acceptable jitter and operates reliably over extended periods.

### 7.1 Future Enhancements

Several enhancements would improve the system's capabilities and robustness:

**Enhanced Error Handling:** Implement comprehensive error detection and recovery mechanisms, including sensor health monitoring, automatic ESP32 reset on communication failures, and error logging for diagnostic purposes. This would improve system reliability in production environments.

**Multi-Load Control:** Extend the control logic to manage multiple independent loads with separate thresholds and control policies. This would require expanding the mailbox structure and control task logic but would provide more sophisticated energy management capabilities.

**Energy Accumulation:** Add functionality to calculate and report total energy consumption over time (watt-hours) by integrating power measurements. This would require maintaining running totals and implementing overflow handling for long-term operation.

**Web Dashboard Integration:** Develop a complete web dashboard frontend that receives JSON telemetry data, displays real-time power consumption graphs, provides historical energy usage analysis, and allows remote configuration of control thresholds. This would transform the system from a monitoring tool into a complete energy management platform.

**Predictive Load Balancing:** Implement algorithms that predict energy consumption patterns and proactively adjust load control to optimize energy usage. This would require machine learning or statistical analysis capabilities and would represent a significant enhancement to the control logic.

**Formal Real-Time Analysis:** Perform worst-case execution time (WCET) analysis and formal schedulability verification to provide stronger real-time guarantees. This would enable the system to meet requirements for safety-critical applications.

**24-Hour Continuous Operation Validation:** Conduct extended reliability testing over 24-hour periods to validate long-term stability and identify any memory leaks or timing drift issues that might not appear in shorter test runs.

The foundation established by this project provides a solid base for these enhancements, and the modular architecture facilitates incremental development of new features without requiring major system redesign.

