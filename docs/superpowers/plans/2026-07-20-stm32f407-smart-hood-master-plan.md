# STM32F407 Smart Hood Master Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Guide the user through building and validating the STM32F407 FreeRTOS smart range-hood controller one independently testable milestone at a time.

**Architecture:** STM32CubeMX generates HAL and CMSIS-RTOS v2 infrastructure for Keil5. Handwritten code is separated into App, BSP, and Control modules; all operating modes produce a target RPM that is enforced by one encoder-feedback PID controller.

**Tech Stack:** STM32F407VET6, STM32CubeMX, Keil MDK-ARM V5, STM32 HAL, FreeRTOS/CMSIS-RTOS v2, C, ST7735S, DHT11, MQ-2, TB6612FNG, AB encoder

---

## How This Plan Is Used

This is the project roadmap. Only the current milestone receives a detailed click-by-click implementation plan. The next milestone plan is written after the current milestone passes its hardware acceptance tests, because later timer values, screen offsets and control constants depend on measured results.

The assistant teaches and reviews each step; the user performs CubeMX configuration, writes the code in Keil5, builds, flashes and reports observed results. The assistant updates the living documents after each checkpoint.

## Planned Project Layout

```text
firmware/SmartHood/
├─ SmartHood.ioc
├─ Core/
├─ Drivers/
├─ Middlewares/
├─ MDK-ARM/
├─ App/Inc/
├─ App/Src/
├─ BSP/Inc/
├─ BSP/Src/
├─ Control/Inc/
└─ Control/Src/
docs/
├─ project-guide.md
├─ hardware-connections.md
├─ test-records.md
└─ superpowers/
   ├─ specs/
   └─ plans/
```

### Task 1: M1 — CubeMX and FreeRTOS Minimum System

**Purpose:** Establish a known-good 168 MHz HAL + FreeRTOS project before external modules are connected.

**Deliverables:**

- `firmware/SmartHood/SmartHood.ioc`
- A Keil5 target that builds with zero errors
- USART1 heartbeat output at115200 baud
- PA1 board LED heartbeat
- PA0 raw key-state observation
- M1 test record

**Acceptance:** Run for at least10 minutes with periodic serial output and no unexpected reset.

**Detailed plan:** `docs/superpowers/plans/2026-07-20-m1-cubemx-freertos-bringup.md`

### Task 2: M2 — ST7735S Display

**Purpose:** Connect the purchased display through expansion headers rather than the incompatible board P2 socket.

**Planned resources:**

- SPI2 SCK: PB13
- SPI2 MOSI: PB15
- CS: PD7
- DC: PD6
- RST: PD5
- BL: PD4

**Implementation units:**

- `BSP/Inc/bsp_st7735s.h`
- `BSP/Src/bsp_st7735s.c`
- `BSP/Inc/fonts.h`
- `BSP/Src/fonts.c`

**Tests:** Solid red/green/blue screens, orientation test, coordinate corners, ASCII text, repeated refresh for10 minutes.

**Confirmed module:** 1.8-inch128×160 ST7735S. Pin order is BLK, CS, DC, RST, SDA, SCL, VDD, GND. Use3.3V power; SDA maps toPB15 MOSI and SCL maps toPB13 SCK.

### Task 3: M3 — Sensor Acquisition

**Purpose:** Acquire stable DHT11 data and a safe, filtered MQ-2 relative smoke signal.

**Planned resources:**

- DHT11 DATA: PD0
- MQ-2 AO: PC0 / ADC1_IN10
- TIM5: 1 MHz microsecond time base
- ADC1: 12-bit sampling

**Implementation units:**

- `BSP/Inc/bsp_dht11.h`
- `BSP/Src/bsp_dht11.c`
- `BSP/Inc/bsp_mq2.h`
- `BSP/Src/bsp_mq2.c`

**Tests:** DHT11 valid checksum and disconnect behavior; MQ-2 AO maximum-voltage measurement, warm-up trend and moving-average response.

**Safety gate:** Power the MQ-2 module from5V. Measure AO with a multimeter and pass it through an approximately0.6 ratio divider before PC0. The initial divider is10kΩ from AO toPC0 and15kΩ fromPC0 toGND, limiting a5V input to3.0V.

### Task 4: M4 — TB6612 and Open-Loop Motor Control

**Purpose:** Verify motor power, direction and PWM without PID.

**Planned resources:**

- TIM4_CH1/PB6: PWMA at20 kHz
- PB7: AIN1
- PB8: AIN2
- PB9: STBY

**Implementation units:**

- `BSP/Inc/bsp_motor.h`
- `BSP/Src/bsp_motor.c`

**Tests:** Safe stop, forward rotation, 30/50/70% duty response, reset behavior and supply-voltage drop.

**Confirmed motor:** JGA12-N20-50B, rated6V, ratio50,300 RPM no-load,40mA no-load and0.55A stall. TB6612 current capacity is sufficient. Confirm the actual motor-supply choice and measure the DC-DC outputs before connection.

### Task 5: M5 — Encoder Measurement and Calibration

**Purpose:** Obtain trustworthy output-shaft RPM before closing the loop.

**Planned resources:**

- TIM3_CH1/PC6: encoder A
- TIM3_CH2/PC7: encoder B
- 3.3V encoder supply
- External or verified board pull-ups to3.3V

**Implementation units:**

- `BSP/Inc/bsp_encoder.h`
- `BSP/Src/bsp_encoder.c`

**Tests:** Direction sign, count accumulation, counter wrap, counts per output revolution and RPM comparison against a timed manual revolution or tachometer.

**Initial constant:** 7 PPR × four-edge decoding × ratio50 =1400 counts per output revolution. Verify this value physically before using it for final accuracy claims.

### Task 6: M6 — PID Closed-Loop Speed Control

**Purpose:** Hold190 and220 RPM using a fixed50 ms control interval.

**Implementation units:**

- `Control/Inc/pid.h`
- `Control/Src/pid.c`
- `App/Inc/system_state.h`
- `App/Src/system_state.c`

**Tests:** Step response, steady-state error, mode-change reset, start boost, encoder-loss stop and limited stall test without prolonged blockage.

**Acceptance target:** Steady-state error within±5% after encoder calibration.

### Task 7: M7 — Single-Key Interaction and Mode Manager

**Purpose:** Reliably control the complete system with the one PA0 user key.

**Implementation units:**

- `BSP/Inc/bsp_key.h`
- `BSP/Src/bsp_key.c`
- `Control/Inc/mode_manager.h`
- `Control/Src/mode_manager.c`

**Events:** Long press toggles run/standby; short press cycles AUTO/MANUAL/BACKFLOW; double-click toggles190/220 RPM in MANUAL.

**Tests:** Bounce rejection, double-click arbitration, long-press once-only behavior and mode transition table.

### Task 8: M8 — Sensor Fusion and Backflow Hysteresis

**Purpose:** Generate target RPM from sensors and prevent threshold chatter.

**Implementation units:**

- `Control/Inc/sensor_fusion.h`
- `Control/Src/sensor_fusion.c`

**Initial policy:** Normalize temperature, humidity and MQ-2 relative value; weights0.2/0.2/0.6; map to120–240 RPM. Treat all values as tunable and document final measured values.

**Tests:** Boundary values, sensor failure exclusion, monotonic RPM mapping, hysteresis open/close transitions and noisy threshold input.

### Task 9: M9 — UI, Fault Handling and Stability

**Purpose:** Port LVGL, integrate a coherent display and verify system robustness.

**LVGL scope:** Add the LVGL display port on top of the M2 RGB565 rectangle-write interface, introduce a display buffer and UiTask, and use SPI DMA if the measured refresh cost requires it. Select the exact LVGL version at M9 according to ARMCC5 compatibility and measured Flash/RAM budget.

**Display fields:** Run state, mode, temperature, humidity, smoke-relative value, target RPM, actual RPM, PWM and fault indicator.

**Faults:** DHT11 failure, encoder loss while PWM is active and invalid sensor range.

**Tests:** Sensor disconnects, motor disconnect, repeated mode switching, supply cycling and at least2 hours continuous operation.

### Task 10: M10 — Measurements and Project Handoff

**Purpose:** Convert the working prototype into reproducible documentation and defensible resume evidence.

**Outputs:** Final wiring table, CubeMX screenshots, task timing table, measured RPM error, PID response data, hysteresis test result, known limitations and a concise resume version.

**Documentation:** Update `docs/project-guide.md`, `docs/hardware-connections.md` and `docs/test-records.md`; remove superseded values rather than leaving contradictory parameters.

## Deferred Second Stage

W25Q64 staging, USART + DMA firmware transfer, CRC32 validation and Bootloader jump are intentionally excluded from this master plan. They receive a separate design and implementation plan only after M10 passes.
