**RTOS-Style Scheduler Demo (STM32F446RE)**

*1. Overview*

This project implements a custom RTOS-style cooperative scheduler on the STM32F446RE Nucleo-64 board.
The goal is to demonstrate periodic tasks, inter-task messaging, and UART communication without using FreeRTOS or any external RTOS library.

Three periodic tasks are scheduled:

**TaskSense** — Samples sensor readings (simulated INA219)
**TaskControl** — Applies threshold logic and drives LED (fan indicator)
**TaskComms** — Sends UART messages using mailbox data from Control

This meets the project requirements for:
Multi-task design
Software scheduler
Inter-task communication
UART output

*2. Hardware & Tools*

Board: NUCLEO-F446RE (Nucleo-64)
MCU: STM32F446RET6
IDE: STM32CubeIDE 1.19.0
Clock Source: Internal HSI 16 MHz
UART: USART2 → ST-LINK Virtual COM Port

*3. How to Build and Run*

- Open the project in STM32CubeIDE.
- Place the provided main.c in:
   Core/Src/main.c
- Click Build (hammer icon).
- Click Debug or Run to flash the board.

*4. Serial Output*

Use a serial terminal:

Setting Value:
Baud Rate:	115200
Data:	8 bits
Parity:	None
Stop Bits:	1
Flow Control:	None

Typical output:

RTOS-style 3-task demo start
t=501ms pA=250 pB=750 fan=1
t=1001ms pA=495 pB=505 fan=0
t=1501ms pA=740 pB=260 fan=1

*5. Scheduler Architecture*
   
Cooperative Scheduler:
The scheduler is time-based and uses HAL_GetTick() for 1 ms timing.
Each task is stored in a table:
typedef struct {
    task_fn_t  fn;
    uint32_t   period_ms;
    uint32_t   next_release;
} task_t;


Periods used:

| Task        | Period | Frequency |
| ----------- | ------ | --------- |
| TaskSense   | 1 ms   | 1 kHz     |
| TaskControl | 10 ms  | 100 Hz    |
| TaskComms   | 500 ms | 2 Hz      |

Scheduling Loop

Every 1 ms:

scheduler();
HAL_Delay(1);

The scheduler checks which tasks are due and runs them cooperatively.


*6. Inter-Task Communication (Mailbox)*

TaskControl produces messages, TaskComms consumes them.

typedef struct {
    uint8_t  full;
    uint32_t ticks;
    uint16_t pA, pB;
    uint8_t  fan;
} comms_mailbox_t;


- full = 1 → new data available
- Comms reads the message and prints it
- Sets full = 0 after consuming
  
This satisfies the rubric’s requirement for message passing between tasks.

*7. LED Behavior*

- LD2 (PA5) acts as a fan indicator.
- Turns ON when either sensor reading rises above 600.
- Turns OFF otherwise.

*8. Folder / File Structure*
/Core
  /Inc
  /Src
    main.c
README.md   <-- this file

*9. How to Demonstrate to Instructor*

- Reset the board.
- Show the UART output updating every 500 ms.
- Point out the individual task periods (1 ms, 10 ms, 500 ms).
- Show mailbox operation (Comms prints only when Control posts new data).
- Show LED toggling according to the threshold logic.

This covers all grading criteria for the RTOS demo.
