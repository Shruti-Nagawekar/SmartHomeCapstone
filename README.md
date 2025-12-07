README – RTOS-Style Scheduler Demo (STM32F446RE)
1. Overview

This project implements a student-built RTOS-style cooperative scheduler on the STM32F446RE Nucleo-64 board.
It demonstrates periodic task scheduling, inter-task communication, and UART reporting without using any external RTOS libraries.

Three tasks run concurrently:

TaskSense → Samples power readings (simulated INA219)

TaskControl → Applies threshold logic + drives LED (fan indicator)

TaskComms → Sends updates to UART using a mailbox message from Control

This satisfies the project requirements for a multi-task system, software scheduler, and inter-task messaging.

2. Board & Toolchain

Board: NUCLEO-F446RE (Nucleo-64)

Tool: STM32CubeIDE 1.19.0

Clock: Internal HSI (16 MHz)

UART: USART2 → ST-LINK Virtual COM Port

3. Build Instructions

Open the project inside STM32CubeIDE.

Place the provided main.c inside:

Core/Src/main.c


Press Build (hammer).

Press Debug or Run to flash to the board.

4. Serial Output (UART)

Open a serial terminal such as PuTTY or the CubeIDE terminal:

Port: ST-LINK Virtual COM Port

Baud: 115200

Data: 8-N-1

Flow control: None

Typical output:

RTOS-style 3-task demo start
t=501ms pA=250 pB=750 fan=1
t=1001ms pA=495 pB=505 fan=0
t=1501ms pA=740 pB=260 fan=1
...

5. Scheduler Design

The scheduler is:

Cooperative

Time-based using HAL_GetTick()

Works with a task table, where each task has:

Function pointer

Period in milliseconds

Next scheduled release time

The scheduler runs every 1 ms, checking if any task is due.
This achieves deterministic periodic task activation without preemption.

6. Inter-Task Communication

A mailbox structure carries messages from TaskControl → TaskComms:

typedef struct {
    uint8_t  full;
    uint32_t ticks;
    uint16_t pA, pB;
    uint8_t  fan;
} comms_mailbox_t;


TaskControl publishes the newest sensor + fan state.

TaskComms consumes the message and prints it.

This satisfies the “inter-task communication” rubric requirement.

7. LED Behavior

PA5 LED = Fan indicator

Turns ON when either power channel exceeds threshold (600).

Turns OFF otherwise.

8. File List (submitted)

Core/Src/main.c

README.md (this file)

9. How to Demonstrate

Reset the board.

Show serial output updating every 500 ms.

Point out the three tasks and their periods.

Show mailbox operation (only prints when Control updates mailbox).

Show LED reacting to simulated load changes.

This completes the functional requirements for a student-designed RTOS-style scheduler.
