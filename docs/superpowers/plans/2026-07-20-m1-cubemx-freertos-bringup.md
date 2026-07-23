# M1 CubeMX and FreeRTOS Bring-Up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create and validate the first STM32F407VET6 CubeMX/Keil5 firmware with a168 MHz clock, FreeRTOS, USART1 logging, PA1 LED heartbeat and PA0 key observation.

**Architecture:** CubeMX owns clock, GPIO, UART, HAL time base and FreeRTOS initialization. Handwritten logging and application-loop code live outside generated functions and are called only through CubeMX USER CODE sections.

**Tech Stack:** STM32CubeMX, STM32F407VET6, STM32 HAL, FreeRTOS with CMSIS-RTOS v2, USART1, Keil MDK-ARM V5

---

## M1 Safety and Success Criteria

Do not connect MQ-2, DHT11, TB6612, motor, encoder or TFT during M1. Power only the minimum system board, ST-Link and optional USB-to-UART adapter.

M1 passes only when:

- Keil reports `0 Error(s)`.
- USART1 prints a heartbeat once per second at115200 baud.
- PA1 LED changes state once per second.
- Pressing PA0 changes the reported raw key value from0 to1.
- The firmware runs for at least10 minutes without restarting its heartbeat counter.

### Task 1: Prepare the Workspace and Connections

**Files:**

- Verify: `docs/hardware-connections.md`
- Verify: `stm32f407vet6.pdf`
- Create later through CubeMX: `firmware/SmartHood/SmartHood.ioc`

- [ ] **Step 1: Create the firmware parent folder**

In File Explorer, create:

```text
D:\Keil5 prj\stm32f4\firmware
```

Do not manually create `SmartHood`; CubeMX will create the project folder.

- [ ] **Step 2: Connect ST-Link with the board powered off**

```text
ST-Link SWDIO  → board PA13/SWDIO
ST-Link SWCLK  → board PA14/SWCLK
ST-Link GND    → board GND
ST-Link 3.3V   → board 3.3V reference only if required by the ST-Link
```

Use one defined board-power source. Do not simultaneously feed the board from several5V sources unless their design explicitly supports it.

- [ ] **Step 3: Connect the USB-to-UART adapter**

For receive-only logging:

```text
USB-UART RX → PA9 / USART1_TX
USB-UART GND → board GND
```

For later bidirectional communication, also connect:

```text
USB-UART TX → PA10 / USART1_RX
```

Do not connect the adapter5V pin when the board already has power.

- [ ] **Step 4: Record the initial setup**

Add an M1 entry to `docs/test-records.md` containing the power method, ST-Link model and USB-UART logic voltage. Leave the result as `未测试` until the firmware runs.

### Task 2: Create the CubeMX Project

**Files:**

- Create: `firmware/SmartHood/SmartHood.ioc`

- [ ] **Step 1: Select the MCU**

Open STM32CubeMX, choose `Access to MCU Selector`, and select:

```text
STM32F407VETx
Package: LQFP100
Flash: 512 Kbytes
```

Confirm that the selected part is the `VE` variant, not `VG`, `ZE` or another package.

- [ ] **Step 2: Set the project name and location**

In `Project Manager` use:

```text
Project Name: SmartHood
Project Location: D:\Keil5 prj\stm32f4\firmware
Toolchain / IDE: MDK-ARM
Minimum Heap Size: 0x400
Minimum Stack Size: 0x800
```

- [ ] **Step 3: Configure debug access**

Open `System Core → SYS`:

```text
Debug: Serial Wire
Timebase Source: TIM6
```

Serial Wire preserves PA13/PA14 debugging and releases JTAG-only pins such as PB3/PB4 for the board Flash in a later milestone.

- [ ] **Step 4: Configure the external clock source**

Open `System Core → RCC`:

```text
High Speed Clock (HSE): Crystal/Ceramic Resonator
Low Speed Clock (LSE): Disable for M1
```

The board schematic shows an8 MHz HSE crystal. LSE is physically present but unnecessary for the first milestone.

### Task 3: Configure the 168 MHz Clock Tree

**Files:**

- Modify through CubeMX: `firmware/SmartHood/SmartHood.ioc`

- [ ] **Step 1: Select HSE as the PLL source**

Open `Clock Configuration` and enter:

```text
HSE input frequency: 8 MHz
PLL Source Mux: HSE
PLLM: 8
PLLN: 336
PLLP: 2
PLLQ: leave CubeMX-managed in M1; if a future USB/SDIO/RNG peripheral enables the 48 MHz domain, set PLLQ to 7
System Clock Mux: PLLCLK
```

- [ ] **Step 2: Set the bus prescalers**

Confirm the displayed clocks are:

```text
SYSCLK: 168 MHz
HCLK/AHB: 168 MHz
APB1 peripheral clock: 42 MHz
APB1 timer clock: 84 MHz
APB2 peripheral clock: 84 MHz
APB2 timer clock: 168 MHz
USB/SDIO/RNG clock: not required in M1; it may remain grey because no consumer is enabled
```

- [ ] **Step 3: Resolve CubeMX warnings before continuing**

The clock page must contain no red invalid-frequency field. If CubeMX offers automatic conflict resolution, compare the resulting PLL values with the values above instead of accepting an unexplained configuration.

### Task 4: Configure GPIO and USART1

**Files:**

- Modify through CubeMX: `firmware/SmartHood/SmartHood.ioc`

- [ ] **Step 1: Configure the board LED**

Select PA1 and choose `GPIO_Output`. In GPIO settings use:

```text
User Label: BOARD_LED
GPIO output level: High
GPIO mode: Output Push Pull
Pull-up/Pull-down: No pull-up and no pull-down
Maximum output speed: Low
```

The schematic indicates PA1 is connected to the user LED. The active level will be verified on hardware; toggling works regardless of whether High or Low means illuminated.

- [ ] **Step 2: Configure the board key**

Select PA0 and choose `GPIO_Input`. Use:

```text
User Label: USER_KEY
Pull-up/Pull-down: Pull-down
```

The schematic shows PA0 connected through the board button toward3.3V, so the expected raw value is0 released and1 pressed.

- [ ] **Step 3: Enable USART1**

Open `Connectivity → USART1` and choose `Asynchronous`. Confirm:

```text
TX: PA9
RX: PA10
Baud rate: 115200 Bits/s
Word length: 8 Bits
Parity: None
Stop bits: 1
Hardware flow control: None
Oversampling: 16
```

Do not enable USART interrupts or DMA in M1.

### Task 5: Add FreeRTOS and Generate the Project

**Files:**

- Create through CubeMX: `firmware/SmartHood/Core/**`
- Create through CubeMX: `firmware/SmartHood/Drivers/**`
- Create through CubeMX: `firmware/SmartHood/Middlewares/**`
- Create through CubeMX: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [ ] **Step 1: Enable FreeRTOS**

Open `Middleware and Software Packs → FreeRTOS`:

```text
Interface: CMSIS_V2
TOTAL_HEAP_SIZE: 32768 bytes
Default task name: defaultTask
Default task priority: osPriorityNormal
Default task stack size: 256 words (1024 bytes on Cortex-M4)
```

Keep one default task only. Additional project tasks are introduced in later milestones.

- [ ] **Step 2: Configure code-generation behavior**

Open `Project Manager → Code Generator` and enable:

```text
Keep User Code when re-generating
Generate peripheral initialization as a pair of '.c/.h' files per peripheral
```

Use the STM32Cube firmware package version already installed on the computer and record that version in `docs/project-guide.md` after generation.

- [ ] **Step 3: Generate code**

Click `GENERATE CODE`. If CubeMX asks to download a firmware package, stop and report the exact package name and version before installing it.

- [ ] **Step 4: Perform the untouched first build**

Open:

```text
firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
```

In Keil select `Project → Build Target` or press `F7`.

Expected result:

```text
0 Error(s)
```

Warnings must be copied into the test record rather than ignored. Do not add application code until the generated project builds.

### Task 6: Add a Small Serial Logging Module

**Files:**

- Create: `firmware/SmartHood/App/Inc/debug_log.h`
- Create: `firmware/SmartHood/App/Src/debug_log.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx` through the Keil GUI

- [x] **Step 1: Create the App directories**

Create these folders in File Explorer:

```text
firmware\SmartHood\App\Inc
firmware\SmartHood\App\Src
```

- [x] **Step 2: Create `debug_log.h`**

```c
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

void DebugLog_Printf(const char *format, ...);

#endif
```

- [x] **Step 3: Create `debug_log.c`**

```c
#include "debug_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "usart.h"

void DebugLog_Printf(const char *format, ...)
{
    char buffer[160];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length <= 0)
    {
        return;
    }

    if (length >= (int)sizeof(buffer))
    {
        length = (int)sizeof(buffer) - 1;
    }

    (void)HAL_UART_Transmit(&huart1,
                            (uint8_t *)buffer,
                            (uint16_t)length,
                            100U);
}
```

- [x] **Step 4: Add the module to Keil**

In Keil:

1. Right-click the target and choose `Manage Project Items`.
2. Create a group named `App`.
3. Add `App/Src/debug_log.c` to that group.
4. Open `Options for Target → C/C++ → Include Paths`.
5. Add `..\App\Inc` relative to the `MDK-ARM` directory.

- [x] **Step 5: Build the logging module**

Press `F7`.

Expected result:

```text
0 Error(s)
```

If Keil reports `usart.h: No such file`, verify that CubeMX generated per-peripheral files and that the existing `Core/Inc` include path remains present.

### Task 7: Move the M1 Application Loop into App Code

**Files:**

- Create: `firmware/SmartHood/App/Inc/app_tasks.h`
- Create: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/Core/Src/freertos.c` only inside USER CODE blocks
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx` through the Keil GUI

- [x] **Step 1: Create `app_tasks.h`**

```c
#ifndef APP_TASKS_H
#define APP_TASKS_H

void App_DefaultTask(void *argument);

#endif
```

- [x] **Step 2: Create `app_tasks.c`**

```c
#include "app_tasks.h"

#include <stdint.h>

#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

void App_DefaultTask(void *argument)
{
    uint32_t heartbeat = 0U;

    (void)argument;

    DebugLog_Printf("\r\nSmartHood M1 start\r\n");

    for (;;)
    {
        GPIO_PinState key_state;

        HAL_GPIO_TogglePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin);
        key_state = HAL_GPIO_ReadPin(USER_KEY_GPIO_Port, USER_KEY_Pin);

        DebugLog_Printf("heartbeat=%lu key=%u tick=%lu\r\n",
                        (unsigned long)heartbeat,
                        (unsigned int)key_state,
                        (unsigned long)HAL_GetTick());

        heartbeat++;
        osDelay(1000U);
    }
}
```

- [x] **Step 3: Add `app_tasks.c` to the Keil App group**

Use `Manage Project Items` and add:

```text
..\App\Src\app_tasks.c
```

- [x] **Step 4: Include the application header from a protected block**

In `Core/Src/freertos.c`, locate:

```c
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */
```

Change it to:

```c
/* USER CODE BEGIN Includes */
#include "app_tasks.h"
/* USER CODE END Includes */
```

- [x] **Step 5: Delegate the generated default task**

Find the generated `StartDefaultTask` function. Keep its signature, but make its body:

```c
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  App_DefaultTask(argument);
  /* USER CODE END StartDefaultTask */
}
```

Do not add a second infinite loop after `App_DefaultTask`; that function does not return.

- [x] **Step 6: Build after application integration**

Press `F7`.

Expected result:

```text
0 Error(s)
```

Resolve every compile error before flashing. Do not edit generated code outside USER CODE blocks.

### Task 8: Flash and Verify M1

**Files:**

- Update: `docs/test-records.md`
- Update: `docs/project-guide.md`

- [x] **Step 1: Configure the Keil debugger**

Open `Options for Target → Debug`, select the installed ST-Link debugger and open `Settings`. Confirm `SW` port is selected and that the target device is detected.

- [x] **Step 2: Flash the firmware**

Use `Download` or press `F8`. Reset the board once after programming.

- [x] **Step 3: Open a serial terminal**

Use these settings:

```text
Baud: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
```

Expected output:

```text
SmartHood M1 start
heartbeat=0 key=0 tick=...
heartbeat=1 key=0 tick=...
```

- [x] **Step 4: Verify the key input**

Hold PA0 before the next heartbeat line. Expected:

```text
key=1
```

Release it. Expected:

```text
key=0
```

If the values are reversed or floating, stop and report the observed sequence before changing the pull configuration.

- [x] **Step 5: Verify the board LED**

The PA1 LED should change state once per second. Record whether High or Low is the illuminated state.

- [ ] **Step 6: Run the10-minute stability check**

Allow at least600 heartbeat lines. Pass conditions:

- Counter increases monotonically.
- Tick increases by approximately1000 ms per line.
- No second `SmartHood M1 start` appears unless the board was manually reset.
- The LED continues toggling.

- [x] **Step 7: Update the living documents**

In `docs/project-guide.md`, record:

- CubeMX version.
- STM32CubeF4 package version.
- Keil version.
- FreeRTOS/CMSIS interface.
- M1 status.

In `docs/test-records.md`, record the build summary, serial sample, PA0 result, PA1 active level and10-minute result.

- [x] **Step 8: Stop at the M1 checkpoint**

Do not connect the TFT yet. Send the build output and observed serial lines for review. M2 begins only after M1 is marked passed.
