# M7 Single-Key Interaction and Mode Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 使用唯一的PA0板载按键可靠识别短按、双击和长按，管理待机/运行许可、AUTO/MANUAL/BACKFLOW模式及手动低/高档，同时保持MotorTask对PWM和编码器故障的唯一所有权。

**Architecture:** `BSP_Key_Process()`接收PA0原始电平和毫秒Tick，输出确定性的按键事件；DefaultTask仅把事件发送到CMSIS-RTOS2队列。MotorTask依次消费事件并调用纯状态机`ModeManager`，再把状态快照映射到M6的STOP、软启动、PI和FAULT状态。

**Tech Stack:** STM32F407VET6、STM32CubeF4 HAL 1.28.3、CMSIS-RTOS2/FreeRTOS、TIM3 Encoder Mode、TIM4 PWM、TB6612FNG、Keil MDK 5/ARMCC 5.06。

---

## 文件结构

新增文件：

- `firmware/SmartHood/BSP/Inc/bsp_key.h`：按键事件、上下文和非阻塞处理接口。
- `firmware/SmartHood/BSP/Src/bsp_key.c`：40ms消抖、350ms双击及1000ms长按状态机。
- `firmware/SmartHood/BSP/Test/bsp_key_selftest.h/.c`：固定电平和时间序列自检。
- `firmware/SmartHood/Control/Inc/mode_manager.h`：模式、运行许可、手动挡位、故障和电机请求接口。
- `firmware/SmartHood/Control/Src/mode_manager.c`：纯状态转换实现。
- `firmware/SmartHood/Control/Test/mode_manager_selftest.h/.c`：完整转换表自检。

修改文件：

- `firmware/SmartHood/App/Inc/app_tasks.h`：把队列说明从NEXT命令更新为按键事件。
- `firmware/SmartHood/App/Src/app_tasks.c`：按键事件生产、队列消费、模式日志和电机请求映射。
- `firmware/SmartHood/Core/Src/freertos.c`：仅更新队列初始化注释，不改变任务结构。
- `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`：增加BSP Test目录、6个源文件和包含路径。
- `docs/project-guide.md`：记录M7阶段进度、构建和最终结论。
- `docs/test-records.md`：记录软件自检、无VM和接通VM空载验收。

CubeMX外设配置不变，不新增任务、定时器、中断、DMA或GPIO。PA0继续保持GPIO Input、Pull-down和`USER_KEY`标签。

### Task 1: 建立M7实现基线

**Files:**
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood/SmartHood.hex`
- Modify: `docs/project-guide.md`

- [x] **Step 1: 检查分支和工作区**

Run:

```powershell
git status --short --branch
git log -3 --oneline
```

Expected: 当前分支为`codex/feature-m7-key-mode`，设计提交`5db80b9`存在，工作区无未提交固件修改。

- [x] **Step 2: Rebuild M6基线**

在Keil执行`Rebuild`，或由助手使用隐藏窗口运行UV4：

```powershell
$uv4 = 'D:\Keil5\UV4\UV4.exe'
$project = 'D:\Keil5 prj\stm32f4\firmware\SmartHood\MDK-ARM\SmartHood.uvprojx'
$log = "$env:TEMP\smarthood-m7-baseline.log"
$args = @('-r', ('"' + $project + '"'), '-t', '"SmartHood"', '-o', ('"' + $log + '"'))
Start-Process -FilePath $uv4 -ArgumentList $args -WindowStyle Hidden -Wait
Get-Content $log
```

Expected: `0 Error(s), 0 Warning(s)`，程序尺寸为`Code=28042、RO-data=1422、RW-data=168、ZI-data=39672`。

- [x] **Step 3: 校验基线HEX**

Run:

```powershell
Get-FileHash `
  'firmware\SmartHood\MDK-ARM\SmartHood\SmartHood.hex' `
  -Algorithm SHA256
```

Expected:

```text
F85F226CE1B5F8F3FEBE85360801217742E2ED39B3CA8E6A53313A6F0016CA55
```

- [x] **Step 4: 记录并提交基线**

在`docs/project-guide.md`的M7记录中追加基线构建和HEX。

```powershell
git add docs/project-guide.md
git commit -m "docs: start M7 key and mode implementation"
```

### Task 2: 先写按键接口和失败自检

**Files:**
- Create: `firmware/SmartHood/BSP/Inc/bsp_key.h`
- Create: `firmware/SmartHood/BSP/Test/bsp_key_selftest.h`
- Create: `firmware/SmartHood/BSP/Test/bsp_key_selftest.c`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [x] **Step 1: 建立BSP Test目录和Keil分组**

创建物理目录`firmware/SmartHood/BSP/Test`。在Keil中新增`BSP Test` Group；Include Paths增加：

```text
..\BSP\Test
```

保留已有`..\BSP\Inc`、`..\Control\Inc`和`..\Control\Test`。

- [x] **Step 2: 创建按键公共接口**

创建`BSP/Inc/bsp_key.h`：

```c
/**
 * @file bsp_key.h
 * @brief PA0单按键非阻塞事件识别接口。
 */

#ifndef BSP_KEY_H
#define BSP_KEY_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_KEY_DEBOUNCE_MS     40U
#define BSP_KEY_DOUBLE_CLICK_MS 350U
#define BSP_KEY_LONG_PRESS_MS   1000U

typedef enum
{
    BSP_KEY_EVENT_NONE = 0,
    BSP_KEY_EVENT_SHORT,
    BSP_KEY_EVENT_DOUBLE,
    BSP_KEY_EVENT_LONG
} BSP_KeyEvent_t;

typedef struct
{
    bool candidate_pressed;
    bool stable_pressed;
    bool armed;
    bool press_active;
    bool long_reported;
    bool click_pending;
    bool second_press;
    uint32_t candidate_since_ms;
    uint32_t press_since_ms;
    uint32_t first_release_ms;
} BSP_Key_t;

void BSP_Key_Init(BSP_Key_t *key,
                  bool initial_pressed,
                  uint32_t now_ms);

BSP_KeyEvent_t BSP_Key_Process(BSP_Key_t *key,
                               bool raw_pressed,
                               uint32_t now_ms);

bool BSP_Key_IsPressed(const BSP_Key_t *key);

#endif /* BSP_KEY_H */
```

- [x] **Step 3: 创建自检接口**

创建`BSP/Test/bsp_key_selftest.h`：

```c
/**
 * @file bsp_key_selftest.h
 * @brief 单按键状态机确定性自检入口。
 */

#ifndef BSP_KEY_SELFTEST_H
#define BSP_KEY_SELFTEST_H

#include <stdbool.h>

bool BSP_Key_RunSelfTests(void);

#endif /* BSP_KEY_SELFTEST_H */
```

- [x] **Step 4: 写固定时间序列自检**

创建`BSP/Test/bsp_key_selftest.c`。测试必须分别初始化上下文，覆盖以下断言：

```c
/**
 * @file bsp_key_selftest.c
 * @brief 用固定电平和毫秒Tick验证按键事件识别。
 */

#include "bsp_key_selftest.h"
#include "bsp_key.h"

static bool BSP_Key_TestSingle(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);
    if (BSP_Key_Process(&key, true, 100U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, true, 140U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, false, 200U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, false, 240U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, false, 589U) != BSP_KEY_EVENT_NONE) return false;
    return BSP_Key_Process(&key, false, 590U) == BSP_KEY_EVENT_SHORT;
}

static bool BSP_Key_TestDouble(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);
    (void)BSP_Key_Process(&key, true, 100U);
    (void)BSP_Key_Process(&key, true, 140U);
    (void)BSP_Key_Process(&key, false, 200U);
    (void)BSP_Key_Process(&key, false, 240U);
    (void)BSP_Key_Process(&key, true, 400U);
    (void)BSP_Key_Process(&key, true, 440U);
    (void)BSP_Key_Process(&key, false, 480U);
    return BSP_Key_Process(&key, false, 520U) == BSP_KEY_EVENT_DOUBLE;
}

static bool BSP_Key_TestLongOnce(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);
    (void)BSP_Key_Process(&key, true, 100U);
    (void)BSP_Key_Process(&key, true, 140U);
    if (BSP_Key_Process(&key, true, 1139U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, true, 1140U) != BSP_KEY_EVENT_LONG) return false;
    if (BSP_Key_Process(&key, true, 1500U) != BSP_KEY_EVENT_NONE) return false;
    (void)BSP_Key_Process(&key, false, 1600U);
    return BSP_Key_Process(&key, false, 1640U) == BSP_KEY_EVENT_NONE;
}

static bool BSP_Key_TestStartupHeld(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, true, 0U);
    if (BSP_Key_Process(&key, true, 1200U) != BSP_KEY_EVENT_NONE) return false;
    (void)BSP_Key_Process(&key, false, 1300U);
    if (BSP_Key_Process(&key, false, 1340U) != BSP_KEY_EVENT_NONE) return false;
    (void)BSP_Key_Process(&key, true, 1400U);
    (void)BSP_Key_Process(&key, true, 1440U);
    (void)BSP_Key_Process(&key, false, 1500U);
    (void)BSP_Key_Process(&key, false, 1540U);
    return BSP_Key_Process(&key, false, 1890U) == BSP_KEY_EVENT_SHORT;
}

static bool BSP_Key_TestTickWrap(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0xFFFFFFF0U);
    (void)BSP_Key_Process(&key, true, 0xFFFFFFF5U);
    if (BSP_Key_Process(&key, true, 0x0000001DU) != BSP_KEY_EVENT_NONE) return false;
    return BSP_Key_Process(&key, true, 0x00000405U) == BSP_KEY_EVENT_LONG;
}

static bool BSP_Key_TestBounce(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);
    if (BSP_Key_Process(&key, true, 10U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, false, 20U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, true, 30U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, false, 50U) != BSP_KEY_EVENT_NONE) return false;
    if (BSP_Key_Process(&key, false, 90U) != BSP_KEY_EVENT_NONE) return false;
    return !BSP_Key_IsPressed(&key);
}

bool BSP_Key_RunSelfTests(void)
{
    return BSP_Key_TestBounce() &&
           BSP_Key_TestSingle() &&
           BSP_Key_TestDouble() &&
           BSP_Key_TestLongOnce() &&
           BSP_Key_TestStartupHeld() &&
           BSP_Key_TestTickWrap();
}
```

- [x] **Step 5: 临时调用自检并加入Keil工程**

将`bsp_key_selftest.c`加入`BSP Test` Group；Include Paths已有`..\BSP\Inc;..\BSP\Test`。在`app_tasks.c`包含`bsp_key_selftest.h`，新增：

```c
#define APP_M7_SELF_TEST_ENABLED 1U
```

在MotorTask初始化电机之前调用：

```c
#if APP_M7_SELF_TEST_ENABLED
    if (!BSP_Key_RunSelfTests())
    {
        DebugLog_Printf("M7 key self-test FAILED\r\n");
        BSP_Motor_Stop();
        for (;;) { osDelay(1000U); }
    }
    DebugLog_Printf("M7 key self-test PASSED\r\n");
#endif
```

- [x] **Step 6: Rebuild并确认红灯**

Expected: 链接失败，未定义符号只来自尚未实现的：

```text
BSP_Key_Init
BSP_Key_Process
BSP_Key_IsPressed
```

记录错误数量和符号；此时不能提交“通过”。

- [x] **Step 7: 提交红灯测试**

```powershell
git add firmware/SmartHood/BSP firmware/SmartHood/App/Src/app_tasks.c firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
git commit -m "test: add M7 key event self-test"
```

### Task 3: 实现按键状态机并取得绿灯

**Files:**
- Create: `firmware/SmartHood/BSP/Src/bsp_key.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`
- Modify: `docs/test-records.md`

- [x] **Step 1: 实现非阻塞按键状态机**

创建`BSP/Src/bsp_key.c`：

```c
/**
 * @file bsp_key.c
 * @brief PA0单按键消抖、单击、双击和长按状态机。
 */

#include "bsp_key.h"

#include <stddef.h>

static uint32_t BSP_Key_Elapsed(uint32_t now_ms, uint32_t since_ms)
{
    return (uint32_t)(now_ms - since_ms);
}

void BSP_Key_Init(BSP_Key_t *key,
                  bool initial_pressed,
                  uint32_t now_ms)
{
    if (key == NULL) return;

    key->candidate_pressed = initial_pressed;
    key->stable_pressed = initial_pressed;
    key->armed = !initial_pressed;
    key->press_active = false;
    key->long_reported = false;
    key->click_pending = false;
    key->second_press = false;
    key->candidate_since_ms = now_ms;
    key->press_since_ms = now_ms;
    key->first_release_ms = now_ms;
}

BSP_KeyEvent_t BSP_Key_Process(BSP_Key_t *key,
                               bool raw_pressed,
                               uint32_t now_ms)
{
    if (key == NULL) return BSP_KEY_EVENT_NONE;

    if (raw_pressed != key->candidate_pressed)
    {
        key->candidate_pressed = raw_pressed;
        key->candidate_since_ms = now_ms;
    }

    if ((key->candidate_pressed != key->stable_pressed) &&
        (BSP_Key_Elapsed(now_ms, key->candidate_since_ms) >=
         BSP_KEY_DEBOUNCE_MS))
    {
        key->stable_pressed = key->candidate_pressed;

        if (!key->armed)
        {
            if (!key->stable_pressed)
            {
                key->armed = true;
                key->click_pending = false;
                key->second_press = false;
            }
            return BSP_KEY_EVENT_NONE;
        }

        if (key->stable_pressed)
        {
            key->press_active = true;
            key->press_since_ms = now_ms;
            key->long_reported = false;

            if (key->click_pending)
            {
                if (BSP_Key_Elapsed(key->candidate_since_ms,
                                    key->first_release_ms) <=
                    BSP_KEY_DOUBLE_CLICK_MS)
                {
                    key->second_press = true;
                }
                else
                {
                    key->click_pending = false;
                    key->second_press = false;
                    return BSP_KEY_EVENT_SHORT;
                }
            }
        }
        else
        {
            key->press_active = false;

            if (key->long_reported)
            {
                key->long_reported = false;
                key->second_press = false;
                return BSP_KEY_EVENT_NONE;
            }

            if (key->second_press)
            {
                key->click_pending = false;
                key->second_press = false;
                return BSP_KEY_EVENT_DOUBLE;
            }

            key->click_pending = true;
            key->first_release_ms = now_ms;
        }
    }

    if (key->armed && key->stable_pressed &&
        key->press_active && !key->long_reported &&
        (BSP_Key_Elapsed(now_ms, key->press_since_ms) >=
         BSP_KEY_LONG_PRESS_MS))
    {
        key->long_reported = true;
        key->click_pending = false;
        key->second_press = false;
        return BSP_KEY_EVENT_LONG;
    }

    if (key->click_pending && !key->stable_pressed &&
        !key->candidate_pressed &&
        (BSP_Key_Elapsed(now_ms, key->first_release_ms) >=
         BSP_KEY_DOUBLE_CLICK_MS))
    {
        key->click_pending = false;
        return BSP_KEY_EVENT_SHORT;
    }

    return BSP_KEY_EVENT_NONE;
}

bool BSP_Key_IsPressed(const BSP_Key_t *key)
{
    return (key != NULL) && key->stable_pressed;
}
```

- [x] **Step 2: 将实现加入BSP Group并Rebuild**

Expected: `bsp_key.c`和`bsp_key_selftest.c`均参与编译；`0 Error(s), 0 Warning(s)`。

- [x] **Step 3: VM保持断开，烧录自检固件**

Expected串口：

```text
M7 key self-test PASSED
```

电机保持停止，DHT11、heartbeat、PA1和TFT继续工作。

- [x] **Step 4: 记录并提交按键绿灯**

```powershell
git add firmware/SmartHood/BSP firmware/SmartHood/MDK-ARM/SmartHood.uvprojx docs/test-records.md
git commit -m "feat: add M7 key event recognizer"
```

### Task 4: 先写模式管理失败自检

**Files:**
- Create: `firmware/SmartHood/Control/Inc/mode_manager.h`
- Create: `firmware/SmartHood/Control/Test/mode_manager_selftest.h`
- Create: `firmware/SmartHood/Control/Test/mode_manager_selftest.c`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [x] **Step 1: 创建模式管理公共接口**

创建`Control/Inc/mode_manager.h`：

```c
/**
 * @file mode_manager.h
 * @brief SmartHood运行许可、模式、手动挡位和故障状态机。
 */

#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdbool.h>
#include "bsp_key.h"

typedef enum { MODE_RUN_STANDBY = 0, MODE_RUN_RUNNING } ModeRunState_t;
typedef enum { MODE_AUTO = 0, MODE_MANUAL, MODE_BACKFLOW } ModeType_t;
typedef enum { MODE_MANUAL_LOW = 0, MODE_MANUAL_HIGH } ModeManualLevel_t;
typedef enum { MODE_FAULT_NONE = 0, MODE_FAULT_ENCODER_TIMEOUT } ModeFault_t;

typedef enum
{
    MODE_RESULT_NONE = 0,
    MODE_RESULT_CHANGED,
    MODE_RESULT_IGNORED_MODE,
    MODE_RESULT_IGNORED_FAULT,
    MODE_RESULT_FAULT_CLEARED
} ModeResult_t;

typedef enum
{
    MODE_MOTOR_STOP = 0,
    MODE_MOTOR_LOW,
    MODE_MOTOR_HIGH,
    MODE_MOTOR_FAULT
} ModeMotorRequest_t;

typedef struct
{
    ModeRunState_t run_state;
    ModeType_t mode;
    ModeManualLevel_t manual_level;
    ModeFault_t fault;
} ModeManager_t;

void ModeManager_Init(ModeManager_t *manager);
ModeResult_t ModeManager_HandleEvent(ModeManager_t *manager,
                                     BSP_KeyEvent_t event);
void ModeManager_SetFault(ModeManager_t *manager, ModeFault_t fault);
ModeMotorRequest_t ModeManager_GetMotorRequest(const ModeManager_t *manager);

#endif /* MODE_MANAGER_H */
```

- [x] **Step 2: 创建完整转换表自检**

创建`Control/Test/mode_manager_selftest.c`，完整内容为：

```c
/**
 * @file mode_manager_selftest.c
 * @brief 使用固定事件验证模式管理转换表。
 */

#include "mode_manager_selftest.h"
#include "mode_manager.h"

bool ModeManager_RunSelfTests(void)
{
    ModeManager_t manager;

    ModeManager_Init(&manager);
    if ((manager.run_state != MODE_RUN_STANDBY) ||
        (manager.mode != MODE_AUTO) ||
        (manager.manual_level != MODE_MANUAL_LOW) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP)) return false;

    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) != MODE_RESULT_CHANGED) return false;
    if (manager.mode != MODE_MANUAL) return false;
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) != MODE_RESULT_CHANGED) return false;
    if (manager.manual_level != MODE_MANUAL_HIGH) return false;
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP) return false;

    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) != MODE_RESULT_CHANGED) return false;
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_HIGH) return false;
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) != MODE_RESULT_CHANGED) return false;
    if ((manager.mode != MODE_BACKFLOW) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP)) return false;
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) != MODE_RESULT_IGNORED_MODE) return false;

    ModeManager_SetFault(&manager, MODE_FAULT_ENCODER_TIMEOUT);
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_FAULT) return false;
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) != MODE_RESULT_IGNORED_FAULT) return false;
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) != MODE_RESULT_IGNORED_FAULT) return false;
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) != MODE_RESULT_FAULT_CLEARED) return false;

    return (manager.run_state == MODE_RUN_STANDBY) &&
           (manager.mode == MODE_AUTO) &&
           (manager.manual_level == MODE_MANUAL_LOW) &&
           (manager.fault == MODE_FAULT_NONE) &&
           (ModeManager_GetMotorRequest(&manager) == MODE_MOTOR_STOP);
}
```

创建`Control/Test/mode_manager_selftest.h`：

```c
/**
 * @file mode_manager_selftest.h
 * @brief 模式管理状态转换表自检入口。
 */

#ifndef MODE_MANAGER_SELFTEST_H
#define MODE_MANAGER_SELFTEST_H

#include <stdbool.h>

bool ModeManager_RunSelfTests(void);

#endif /* MODE_MANAGER_SELFTEST_H */
```

- [x] **Step 3: 加入Control Test并临时调用**

将`mode_manager_selftest.c`加入已有`Control Test` Group。在`app_tasks.c`包含`mode_manager_selftest.h`，并在M7自检块中同时执行两个测试：

```c
if (!BSP_Key_RunSelfTests() || !ModeManager_RunSelfTests())
{
    DebugLog_Printf("M7 self-test FAILED\r\n");
    BSP_Motor_Stop();
    for (;;) { osDelay(1000U); }
}
DebugLog_Printf("M7 self-test PASSED\r\n");
```

- [x] **Step 4: Rebuild并确认红灯**

Expected: 链接失败，未定义符号只来自：

```text
ModeManager_Init
ModeManager_HandleEvent
ModeManager_SetFault
ModeManager_GetMotorRequest
```

- [x] **Step 5: 提交模式红灯测试**

```powershell
git add firmware/SmartHood/Control firmware/SmartHood/App/Src/app_tasks.c firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
git commit -m "test: add M7 mode manager self-test"
```

### Task 5: 实现模式管理器并取得绿灯

**Files:**
- Create: `firmware/SmartHood/Control/Src/mode_manager.c`
- Modify: `firmware/SmartHood/Control/Inc/mode_manager.h`
- Modify: `firmware/SmartHood/Control/Test/mode_manager_selftest.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`
- Modify: `docs/test-records.md`

- [x] **Step 1: 实现纯模式状态机**

创建`Control/Src/mode_manager.c`：

```c
/**
 * @file mode_manager.c
 * @brief SmartHood模式转换和电机请求映射。
 */

#include "mode_manager.h"

#include <stddef.h>

void ModeManager_Init(ModeManager_t *manager)
{
    if (manager == NULL) return;
    manager->run_state = MODE_RUN_STANDBY;
    manager->mode = MODE_AUTO;
    manager->manual_level = MODE_MANUAL_LOW;
    manager->fault = MODE_FAULT_NONE;
}

ModeResult_t ModeManager_HandleEvent(ModeManager_t *manager,
                                     BSP_KeyEvent_t event)
{
    if ((manager == NULL) || (event == BSP_KEY_EVENT_NONE))
        return MODE_RESULT_NONE;

    if (manager->fault != MODE_FAULT_NONE)
    {
        if (event == BSP_KEY_EVENT_LONG)
        {
            ModeManager_Init(manager);
            return MODE_RESULT_FAULT_CLEARED;
        }
        return MODE_RESULT_IGNORED_FAULT;
    }

    if (event == BSP_KEY_EVENT_LONG)
    {
        manager->run_state =
            (manager->run_state == MODE_RUN_STANDBY) ?
            MODE_RUN_RUNNING : MODE_RUN_STANDBY;
        return MODE_RESULT_CHANGED;
    }

    if (event == BSP_KEY_EVENT_SHORT)
    {
        if (manager->mode == MODE_AUTO) manager->mode = MODE_MANUAL;
        else if (manager->mode == MODE_MANUAL) manager->mode = MODE_BACKFLOW;
        else manager->mode = MODE_AUTO;
        return MODE_RESULT_CHANGED;
    }

    if (event == BSP_KEY_EVENT_DOUBLE)
    {
        if (manager->mode != MODE_MANUAL)
            return MODE_RESULT_IGNORED_MODE;
        manager->manual_level =
            (manager->manual_level == MODE_MANUAL_LOW) ?
            MODE_MANUAL_HIGH : MODE_MANUAL_LOW;
        return MODE_RESULT_CHANGED;
    }

    return MODE_RESULT_NONE;
}

void ModeManager_SetFault(ModeManager_t *manager, ModeFault_t fault)
{
    /* 空上下文不处理；清故障时完整恢复安全初始状态。 */
    if (manager == NULL) return;
    if (fault == MODE_FAULT_NONE)
    {
        ModeManager_Init(manager);
        return;
    }

    /* 非NONE故障直接锁存，电机请求将优先映射为FAULT。 */
    manager->fault = fault;
}

ModeMotorRequest_t ModeManager_GetMotorRequest(const ModeManager_t *manager)
{
    if (manager == NULL) return MODE_MOTOR_STOP;
    if (manager->fault != MODE_FAULT_NONE) return MODE_MOTOR_FAULT;
    if ((manager->run_state != MODE_RUN_RUNNING) ||
        (manager->mode != MODE_MANUAL)) return MODE_MOTOR_STOP;

    /* 仅认可显式LOW和HIGH，非法挡位必须安全停止。 */
    if (manager->manual_level == MODE_MANUAL_HIGH)
        return MODE_MOTOR_HIGH;
    if (manager->manual_level == MODE_MANUAL_LOW)
        return MODE_MOTOR_LOW;
    return MODE_MOTOR_STOP;
}
```

当前`mode_manager_selftest.c`还覆盖NULL安全、NONE事件状态不变、
API清故障完整安全复位、非法运行状态/模式/挡位安全停机、
故障中事件不改变完整状态快照，以及独立和连续的完整转换组合。

- [x] **Step 2: 加入Control Group并Rebuild**

Expected: `mode_manager.c`和两个M7自检源文件参与编译；`0 Error(s), 0 Warning(s)`。

- [x] **Step 3: VM断开时烧录绿灯固件**

Expected串口：`M7 self-test PASSED`，其余M6回归功能继续运行。

- [x] **Step 4: 记录并提交模式绿灯**

```powershell
git add firmware/SmartHood/Control firmware/SmartHood/MDK-ARM/SmartHood.uvprojx docs/test-records.md
git commit -m "feat: add M7 mode manager"
```

### Task 6: 把DefaultTask迁移为按键事件生产者

**Files:**
- Modify: `firmware/SmartHood/App/Inc/app_tasks.h`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/Core/Src/freertos.c`

- [x] **Step 1: 将队列元素改为BSP_KeyEvent_t**

删除`App_MotorCommand_t`和`APP_MOTOR_COMMAND_NEXT`。保留长度4，把句柄改名为：

```c
#define APP_KEY_EVENT_QUEUE_LENGTH 4U
static osMessageQueueId_t app_key_event_queue = NULL;
```

`App_MotorControl_Init()`中的创建代码改为：

```c
app_key_event_queue = osMessageQueueNew(
    APP_KEY_EVENT_QUEUE_LENGTH,
    sizeof(BSP_KeyEvent_t),
    NULL);
return app_key_event_queue != NULL;
```

- [x] **Step 2: 用事件发送函数替换NEXT发送函数**

```c
static bool App_PostKeyEvent(BSP_KeyEvent_t event)
{
    if ((app_key_event_queue == NULL) ||
        (event == BSP_KEY_EVENT_NONE)) return false;
    return osMessageQueuePut(app_key_event_queue,
                             &event,
                             0U,
                             0U) == osOK;
}
```

- [x] **Step 3: 用BSP_Key替换DefaultTask内手写消抖**

删除`candidate_since_tick`、`candidate_key_state`和`stable_key_state`。显示自检后初始化：

```c
BSP_Key_t key;
uint32_t now_tick = HAL_GetTick();
bool initial_pressed =
    HAL_GPIO_ReadPin(USER_KEY_GPIO_Port, USER_KEY_Pin) == GPIO_PIN_SET;

BSP_Key_Init(&key, initial_pressed, now_tick);
heartbeat_tick = now_tick;
```

循环中每20ms执行：

```c
BSP_KeyEvent_t event;
bool raw_pressed;

now_tick = HAL_GetTick();
raw_pressed =
    HAL_GPIO_ReadPin(USER_KEY_GPIO_Port, USER_KEY_Pin) == GPIO_PIN_SET;
event = BSP_Key_Process(&key, raw_pressed, now_tick);

if ((event != BSP_KEY_EVENT_NONE) && !App_PostKeyEvent(event))
    DebugLog_Printf("key event queue full\r\n");
```

心跳日志中的按键值改为：

```c
(unsigned int)BSP_Key_IsPressed(&key)
```

- [x] **Step 4: 更新中文注释并静态检查所有权**

Run:

```powershell
rg -n "APP_MOTOR_COMMAND_NEXT|App_MotorPostNextCommand|candidate_key_state|stable_key_state" firmware/SmartHood
rg -n "BSP_Motor_" firmware/SmartHood/App/Src/app_tasks.c
```

Expected: 第一条无匹配；第二条的电机调用只位于MotorTask及其静态辅助函数，不在DefaultTask。

- [x] **Step 5: Rebuild并提交队列迁移**

Expected: `0 Error(s), 0 Warning(s)`。

```powershell
git add firmware/SmartHood/App firmware/SmartHood/Core/Src/freertos.c
git commit -m "refactor: route M7 key events to MotorTask"
```

### Task 7: 将模式请求接入M6电机状态机

**Files:**
- Modify/Test: `firmware/SmartHood/App/Src/app_tasks.c`（包含app内纯映射自检）
- Modify: `docs/test-records.md`

- [x] **Step 1: 在MotorTask初始化ModeManager**

局部变量增加：

```c
ModeManager_t mode_manager;
ModeMotorRequest_t previous_request = MODE_MOTOR_STOP;
```

PI初始化前调用：

```c
ModeManager_Init(&mode_manager);
DebugLog_Printf("mode run=STANDBY mode=AUTO level=LOW fault=NONE\r\n");
```

- [x] **Step 2: 增加状态文本和模式日志辅助函数**

在`app_tasks.c`的电机状态文本函数附近增加：

```c
static const char *App_ModeRunText(ModeRunState_t state)
{
    switch (state)
    {
        case MODE_RUN_STANDBY: return "STANDBY";
        case MODE_RUN_RUNNING: return "RUNNING";
        default: return "INVALID";
    }
}

static const char *App_ModeTypeText(ModeType_t mode)
{
    switch (mode)
    {
        case MODE_AUTO: return "AUTO";
        case MODE_MANUAL: return "MANUAL";
        case MODE_BACKFLOW: return "BACKFLOW";
        default: return "INVALID";
    }
}

static const char *App_ModeLevelText(ModeManualLevel_t level)
{
    switch (level)
    {
        case MODE_MANUAL_LOW: return "LOW";
        case MODE_MANUAL_HIGH: return "HIGH";
        default: return "INVALID";
    }
}

static const char *App_ModeFaultText(ModeFault_t fault)
{
    switch (fault)
    {
        case MODE_FAULT_NONE: return "NONE";
        case MODE_FAULT_ENCODER_TIMEOUT: return "ENCODER_TIMEOUT";
        default: return "INVALID";
    }
}

static const char *App_ModeResultText(ModeResult_t result)
{
    switch (result)
    {
        case MODE_RESULT_NONE: return "NONE";
        case MODE_RESULT_CHANGED: return "CHANGED";
        case MODE_RESULT_IGNORED_MODE: return "IGNORED_MODE";
        case MODE_RESULT_IGNORED_FAULT: return "IGNORED_FAULT";
        case MODE_RESULT_FAULT_CLEARED: return "FAULT_CLEARED";
        default: return "INVALID";
    }
}

static void App_LogModeState(const ModeManager_t *manager,
                             ModeResult_t result)
{
    DebugLog_Printf(
        "mode run=%s mode=%s level=%s fault=%s result=%s\r\n",
        App_ModeRunText(manager->run_state),
        App_ModeTypeText(manager->mode),
        App_ModeLevelText(manager->manual_level),
        App_ModeFaultText(manager->fault),
        App_ModeResultText(result));
}
```

- [x] **Step 3: 每周期取完当前事件队列**

用以下循环替换原来“每周期最多一个NEXT命令”的分支：

```c
BSP_KeyEvent_t event;

while (osMessageQueueGet(app_key_event_queue,
                         &event,
                         NULL,
                         0U) == osOK)
{
    ModeResult_t result =
        ModeManager_HandleEvent(&mode_manager, event);
    App_LogModeState(&mode_manager, result);
}
```

- [x] **Step 4: 用经过自检的纯映射统一进入软启动或停止**

在`app_tasks.c`内增加`App_MotorRequestAction_t`和纯函数
`App_MotorRequestToAction()`；action包含内部状态、初始占空比和立即停机标志。
非法请求必须安全映射为STOP、0%和立即停机。

先增加`App_MotorModeIntegration_RunSelfTests()`并接入M7组合自检，覆盖
STOP、LOW、HIGH、FAULT和非法请求，再通过缺失helper的完整Rebuild取得
`L6218E Undefined symbol App_MotorRequestToAction`红灯。

GREEN阶段只实现这一套映射；请求变化时统一复位PI和无反馈计数，生产路径
根据action只调用`BSP_Motor_SetDuty()`或`BSP_Motor_Stop()`，再更新
`state`、`duty_percent`和`previous_request`。LOW/HIGH继续使用30%软启动。

请求变化时设置`skip_log_sample`；该周期继续完成控制和故障处理，但不把
属于前一50ms状态的编码器delta计入日志。周期末统一清空三个平均日志
累计量，新窗口从下一完整50ms周期开始，避免跨状态混窗。

- [x] **Step 5: 把编码器超时同步给ModeManager**

触发无反馈故障时，在停止电机前增加：

```c
ModeManager_SetFault(&mode_manager,
                     MODE_FAULT_ENCODER_TIMEOUT);
previous_request = MODE_MOTOR_FAULT;
```

保留`state=FAULT`、停止、PI复位和故障日志，同时清零无反馈计数并设置
`skip_log_sample`；故障锁存周期的delta属于切入FAULT前的50ms，因此周期末
丢弃该样本并清空三个平均日志累计量，新窗口从下一完整FAULT周期开始。
长按清故障后，ModeManager请求变为STOP，下一周期把电机状态同步回STOP。

- [x] **Step 6: 删除旧NEXT顺序状态转换并Rebuild**

Run:

```powershell
rg -n "NEXT|APP_MOTOR_COMMAND|App_MotorPostNext" firmware/SmartHood/App
```

Expected: 代码中无旧命令路径；app内纯映射自检参与M7组合自检，生产路径
没有复制第二套映射；Rebuild为`0 Error(s), 0 Warning(s)`。

- [x] **Step 7: 提交M7集成**

```powershell
git add firmware/SmartHood/App/Src/app_tasks.c docs/test-records.md
git commit -m "feat: integrate M7 mode state with motor control"
```

### Task 8: 自检烧录与无VM板端验收

**Files:**
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `docs/test-records.md`

- [ ] **Step 1: VM断开并烧录自检版本**

确认TB6612 VM断开，保留STM32、ST-Link、USB转TTL、TFT、DHT11和逻辑线。烧录`APP_M7_SELF_TEST_ENABLED=1U`的固件。

Expected: `M7 self-test PASSED`；上电模式为`STANDBY AUTO LOW`。

- [ ] **Step 2: 验证短按模式循环**

每次短按后等待至少1秒，预期：

```text
AUTO → MANUAL → BACKFLOW → AUTO
```

每次只变化一个模式，电机保持停止。

- [ ] **Step 3: 验证MANUAL待机双击**

切到MANUAL，在350ms窗口内完成双击。预期`LOW ↔ HIGH`只切换一次，不切换模式，也不启动电机。

- [ ] **Step 4: 验证长按一次性和无反馈故障**

长按约1秒进入RUNNING；释放后不出现额外SHORT。切到MANUAL后先进入软启动，再因VM断开进入`ENCODER_TIMEOUT`。

- [ ] **Step 5: 验证故障交互**

故障中短按和双击日志为`IGNORED_FAULT`。长按清故障后必须回到：

```text
STANDBY AUTO LOW NONE
```

电机保持停止，不自动重启。

- [ ] **Step 6: 关闭临时自检开关并最终Rebuild**

把：

```c
#define APP_M7_SELF_TEST_ENABLED 1U
```

改为：

```c
#define APP_M7_SELF_TEST_ENABLED 0U
```

Expected: `0 Error(s), 0 Warning(s)`。

- [ ] **Step 7: 记录并提交无VM验收**

```powershell
git add firmware/SmartHood/App/Src/app_tasks.c docs/test-records.md
git commit -m "test: verify M7 key and fault interaction"
```

### Task 9: 接通VM空载验收与分支收尾

**Files:**
- Modify: `docs/project-guide.md`
- Modify: `docs/test-records.md`
- Modify: `docs/superpowers/plans/2026-08-16-m7-single-key-mode-manager.md`

- [ ] **Step 1: 完全断电后恢复VM**

禁止安装扇叶或机械负载。确认电机、编码器和TB6612仍使用M6已验证接线；完全断电时恢复VM，然后先给STM32上电确认STANDBY，再接通9V。

- [ ] **Step 2: 验证MANUAL低档和高档**

长按进入RUNNING AUTO，短按进入MANUAL。预期低档先30%软启动300ms后进入LOW PI；双击后重新软启动并进入HIGH PI。每次只切换一个挡位。

- [ ] **Step 3: 验证所有停止路径**

依次验证：

- MANUAL短按切到BACKFLOW立即停止。
- RUNNING长按切到STANDBY立即停止。
- 任意运行状态按RST后恢复`STANDBY AUTO LOW`且不自行启动。

- [ ] **Step 4: 验证系统回归**

确认DHT11持续`status=OK`、heartbeat递增、PA1翻转、USART1无乱码、ST7735S保持正常画面。编码器运行中断线测试仍记录为“未执行”。

- [ ] **Step 5: 完整Rebuild并记录HEX**

Expected: `0 Error(s), 0 Warning(s)`。记录Code/RO/RW/ZI和：

```powershell
Get-FileHash `
  'firmware\SmartHood\MDK-ARM\SmartHood\SmartHood.hex' `
  -Algorithm SHA256
```

- [ ] **Step 6: 更新文档和计划勾选项**

只把实际执行并观察通过的项目标为通过；跳过的编码器断线、扇叶、负载、堵转和温升测试必须继续标为未执行。

- [ ] **Step 7: 创建最终验证提交**

```powershell
git add docs/project-guide.md docs/test-records.md docs/superpowers/plans/2026-08-16-m7-single-key-mode-manager.md
git diff --cached --check
git commit -m "test: validate M7 key and mode management"
```

- [ ] **Step 8: 按开发分支收尾流程合并和推送**

在新鲜Rebuild通过后，把`codex/feature-m7-key-mode`快速合并到`main`，在`main`再次Rebuild，删除已合并分支并推送：

```powershell
git switch main
git pull --ff-only origin main
git merge --ff-only codex/feature-m7-key-mode
git branch -d codex/feature-m7-key-mode
git push origin main
```

最终确认`git status --short --branch`显示`main...origin/main`且工作区为空。
