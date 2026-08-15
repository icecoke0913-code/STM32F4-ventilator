# M6 Relative PI Speed Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不依赖实际 CPR 标定的前提下，实现低档 `130 counts/50 ms`、高档 `195 counts/50 ms` 的相对速度 PI 闭环、软启动和编码器故障锁存。

**Architecture:** `App_DefaultTask`只识别 PA0 并向消息队列发送“下一状态”命令；`App_MotorTask`是唯一允许修改电机 PWM 的任务，以 50 ms 绝对节拍执行状态机、编码器反馈、PI 和故障检测。新增纯整数`control_pi`模块，不访问 HAL 或 FreeRTOS，并先通过板端无电机自检验证。

**Tech Stack:** STM32F407VET6、STM32CubeF4 HAL 1.28.3、CMSIS-RTOS2/FreeRTOS、TIM3 Encoder Mode、TIM4 PWM、TB6612FNG、Keil MDK 5/ARMCC 5.06。

---

## 文件结构

新增文件：

- `firmware/SmartHood/Control/Inc/control_pi.h`：PI 数据结构和公共接口。
- `firmware/SmartHood/Control/Src/control_pi.c`：定点 PI、积分抗饱和和输出限幅。
- `firmware/SmartHood/Control/Test/control_pi_selftest.h`：板端自检入口。
- `firmware/SmartHood/Control/Test/control_pi_selftest.c`：纯算法输入输出测试。

修改文件：

- `firmware/SmartHood/App/Inc/app_tasks.h`：增加控制消息队列初始化接口。
- `firmware/SmartHood/App/Src/app_tasks.c`：PA0 命令发送、控制状态机、PI 和故障日志。
- `firmware/SmartHood/Core/Src/freertos.c`：任务创建前初始化控制消息队列。
- `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`：增加 Control/Test 分组、源文件和包含路径。
- `docs/project-guide.md`：持续记录 M6 进度和最终参数。
- `docs/test-records.md`：记录构建、闭环、阶跃和故障测试结果。

CubeMX 外设配置不变，不新增任务、定时器、中断或 DMA。

### Task 1: 建立 M6 分支与基线

**Files:**
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood/SmartHood.hex`
- Modify: `docs/project-guide.md`

- [x] **Step 1: 检查工作区**

Run:

```powershell
git status --short --branch
git log -3 --oneline
```

Expected: 当前分支为`main`，工作区没有未提交文件。

- [x] **Step 2: 创建功能分支**

Run:

```powershell
git switch -c codex/feature-m6-relative-pi
```

Expected: 当前分支变为`codex/feature-m6-relative-pi`。

- [x] **Step 3: Rebuild M5 基线**

在 Keil 中执行`Rebuild`，或由助手运行：

```powershell
& 'D:\Keil5\UV4\UV4.exe' -r `
  'D:\Keil5 prj\stm32f4\firmware\SmartHood\MDK-ARM\SmartHood.uvprojx' `
  -t 'SmartHood' -j0
```

Expected: `0 Error(s), 0 Warning(s)`；程序尺寸保持`Code=26986、RO-data=1338、RW-data=164、ZI-data=39668`。

- [x] **Step 4: 校验基线 HEX**

Run:

```powershell
Get-FileHash `
  'firmware\SmartHood\MDK-ARM\SmartHood\SmartHood.hex' `
  -Algorithm SHA256
```

Expected:

```text
7E9543FC3A23014FC07FB54B5D550A6986F867067CF8B40C7C0B55F1B8F612E4
```

- [x] **Step 5: 更新并提交启动记录**

在`docs/project-guide.md`记录 M6 分支、基线尺寸和 HEX。

```powershell
git add docs/project-guide.md
git commit -m "docs: start M6 relative PI control"
```

### Task 2: 先写 PI 公共接口和失败自检

**Files:**
- Create: `firmware/SmartHood/Control/Inc/control_pi.h`
- Create: `firmware/SmartHood/Control/Test/control_pi_selftest.h`
- Create: `firmware/SmartHood/Control/Test/control_pi_selftest.c`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [x] **Step 1: 创建 Control 目录**

创建物理目录：

```text
firmware/SmartHood/Control/Inc
firmware/SmartHood/Control/Src
firmware/SmartHood/Control/Test
```

Keil 工程中新增`Control`和`Control Test`两个 Group；Include Paths 增加：

```text
..\Control\Inc
..\Control\Test
```

- [x] **Step 2: 编写 PI 公共接口**

创建`control_pi.h`：

```c
/**
 * @file control_pi.h
 * @brief 相对速度定点 PI 控制器公共接口。
 */

#ifndef CONTROL_PI_H
#define CONTROL_PI_H

#include <stdint.h>

/** Q8 定点缩放，256 表示实数 1.0。 */
#define CONTROL_PI_Q8_SCALE 256L

/**
 * @brief 保存 PI 参数、积分状态和输出范围。
 */
typedef struct
{
    int32_t kp_q8;          /**< 比例系数放大 256 倍。 */
    int32_t ki_q8;          /**< 积分系数放大 256 倍。 */
    int32_t integral;       /**< 累积计数误差。 */
    int32_t integral_min;   /**< 积分下限。 */
    int32_t integral_max;   /**< 积分上限。 */
    int32_t output_min;     /**< PWM 输出下限。 */
    int32_t output_max;     /**< PWM 输出上限。 */
} ControlPi_t;

void ControlPi_Init(ControlPi_t *controller,
                    int32_t kp_q8,
                    int32_t ki_q8,
                    int32_t integral_min,
                    int32_t integral_max,
                    int32_t output_min,
                    int32_t output_max);

void ControlPi_Reset(ControlPi_t *controller);

int32_t ControlPi_Update(ControlPi_t *controller,
                         int32_t target_count,
                         int32_t actual_count,
                         int32_t feedforward);

int32_t ControlPi_GetIntegral(const ControlPi_t *controller);

#endif /* CONTROL_PI_H */
```

- [x] **Step 3: 编写板端自检接口**

创建`control_pi_selftest.h`：

```c
/**
 * @file control_pi_selftest.h
 * @brief PI 纯算法板端自检入口。
 */

#ifndef CONTROL_PI_SELFTEST_H
#define CONTROL_PI_SELFTEST_H

#include <stdbool.h>

bool ControlPi_RunSelfTests(void);

#endif /* CONTROL_PI_SELFTEST_H */
```

- [x] **Step 4: 编写预期失败的自检**

创建`control_pi_selftest.c`：

```c
/**
 * @file control_pi_selftest.c
 * @brief 使用确定输入验证 PI 输出、限幅和积分抗饱和。
 */

#include "control_pi_selftest.h"

#include "control_pi.h"

bool ControlPi_RunSelfTests(void)
{
    ControlPi_t controller;
    int32_t output;
    uint32_t index;

    ControlPi_Init(&controller,
                   64L,
                   4L,
                   -2048L,
                   2048L,
                   30L,
                   90L);

    /* 目标等于反馈时，只输出 50% 前馈。 */
    output = ControlPi_Update(&controller, 130L, 130L, 50L);
    if ((output != 50L) ||
        (ControlPi_GetIntegral(&controller) != 0L))
    {
        return false;
    }

    /* 正误差 10 counts 时，初始输出应为 52%。 */
    ControlPi_Reset(&controller);
    output = ControlPi_Update(&controller, 130L, 120L, 50L);
    if (output != 52L)
    {
        return false;
    }

    /* 负误差 70 counts 时，初始输出应降为 32%。 */
    ControlPi_Reset(&controller);
    output = ControlPi_Update(&controller, 130L, 200L, 50L);
    if (output != 32L)
    {
        return false;
    }

    /* 连续大正误差触及 90% 上限后，积分冻结在 520。 */
    ControlPi_Reset(&controller);
    for (index = 0U; index < 5U; index++)
    {
        output = ControlPi_Update(&controller, 130L, 0L, 50L);
    }
    if ((output != 90L) ||
        (ControlPi_GetIntegral(&controller) != 520L))
    {
        return false;
    }

    /* 大负误差触及 30% 下限时，不继续积累负积分。 */
    ControlPi_Reset(&controller);
    output = ControlPi_Update(&controller, 0L, 300L, 50L);
    if ((output != 30L) ||
        (ControlPi_GetIntegral(&controller) != 0L))
    {
        return false;
    }

    return true;
}
```

- [x] **Step 5: 临时接入自检调用**

在`app_tasks.c`包含区增加：

```c
#include "control_pi.h"
#include "control_pi_selftest.h"
```

增加临时开关：

```c
/** 仅在 M6 PI 模块初次板端验证时设为 1。 */
#define APP_CONTROL_PI_SELF_TEST_ENABLED 1U
```

在`App_MotorTask()`最前面、任何电机初始化之前增加：

```c
#if APP_CONTROL_PI_SELF_TEST_ENABLED
    if (!ControlPi_RunSelfTests())
    {
        DebugLog_Printf("control PI self-test FAILED\r\n");
        BSP_Motor_Stop();

        for (;;)
        {
            osDelay(1000U);
        }
    }

    DebugLog_Printf("control PI self-test PASSED\r\n");
#endif
```

- [x] **Step 6: Rebuild，确认红灯**

暂时不要创建`control_pi.c`。Rebuild。

Expected: 链接失败，至少报告`ControlPi_Init`、`ControlPi_Update`、`ControlPi_Reset`和`ControlPi_GetIntegral`未定义。失败原因必须是实现尚不存在，而不是头文件路径或语法错误。

- [x] **Step 7: 提交失败自检检查点**

```powershell
git add firmware/SmartHood/Control `
        firmware/SmartHood/App/Src/app_tasks.c `
        firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
git commit -m "test: add M6 PI controller self-test"
```

### Task 3: 实现定点 PI 并让自检通过

**Files:**
- Create: `firmware/SmartHood/Control/Src/control_pi.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [x] **Step 1: 编写最小 PI 实现**

创建`control_pi.c`：

```c
/**
 * @file control_pi.c
 * @brief 相对速度 Q8 定点 PI、积分抗饱和与输出限幅。
 */

#include "control_pi.h"

#include <stddef.h>

static int32_t ControlPi_Clamp(int32_t value,
                               int32_t minimum,
                               int32_t maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

void ControlPi_Init(ControlPi_t *controller,
                    int32_t kp_q8,
                    int32_t ki_q8,
                    int32_t integral_min,
                    int32_t integral_max,
                    int32_t output_min,
                    int32_t output_max)
{
    if (controller == NULL)
    {
        return;
    }

    controller->kp_q8 = kp_q8;
    controller->ki_q8 = ki_q8;
    controller->integral = 0L;
    controller->integral_min = integral_min;
    controller->integral_max = integral_max;
    controller->output_min = output_min;
    controller->output_max = output_max;
}

void ControlPi_Reset(ControlPi_t *controller)
{
    if (controller != NULL)
    {
        controller->integral = 0L;
    }
}

int32_t ControlPi_Update(ControlPi_t *controller,
                         int32_t target_count,
                         int32_t actual_count,
                         int32_t feedforward)
{
    int32_t error;
    int32_t candidate_integral;
    int32_t correction;
    int32_t unclamped_output;
    int32_t output;

    if (controller == NULL)
    {
        return 0L;
    }

    error = target_count - actual_count;
    candidate_integral = ControlPi_Clamp(
        controller->integral + error,
        controller->integral_min,
        controller->integral_max);

    correction =
        ((controller->kp_q8 * error) +
         (controller->ki_q8 * candidate_integral)) /
        CONTROL_PI_Q8_SCALE;

    unclamped_output = feedforward + correction;
    output = ControlPi_Clamp(unclamped_output,
                             controller->output_min,
                             controller->output_max);

    /*
     * 输出已饱和且误差仍推动输出继续越界时，不接受本周期积分。
     * 重新使用旧积分计算输出，使积分能够在误差反向后立即退出饱和。
     */
    if (((unclamped_output > controller->output_max) &&
         (error > 0L)) ||
        ((unclamped_output < controller->output_min) &&
         (error < 0L)))
    {
        correction =
            ((controller->kp_q8 * error) +
             (controller->ki_q8 * controller->integral)) /
            CONTROL_PI_Q8_SCALE;

        output = ControlPi_Clamp(feedforward + correction,
                                 controller->output_min,
                                 controller->output_max);
    }
    else
    {
        controller->integral = candidate_integral;
    }

    return output;
}

int32_t ControlPi_GetIntegral(const ControlPi_t *controller)
{
    if (controller == NULL)
    {
        return 0L;
    }

    return controller->integral;
}
```

- [x] **Step 2: 加入 Keil Control Group**

将`control_pi.c`加入`Control` Group，将`control_pi_selftest.c`加入`Control Test` Group。确认两个文件各出现一次。

- [x] **Step 3: Rebuild，确认绿灯**

Expected: `0 Error(s), 0 Warning(s)`。

- [x] **Step 4: 不接电机执行板端自检**

断开 TB6612 VM 或断开电机 AO1/AO2，烧录并打开串口。

Expected:

```text
control PI self-test PASSED
```

不得出现`FAILED`，且电机不得转动。

- [x] **Step 5: 关闭临时自检并再次 Rebuild**

将：

```c
#define APP_CONTROL_PI_SELF_TEST_ENABLED 1U
```

改为：

```c
#define APP_CONTROL_PI_SELF_TEST_ENABLED 0U
```

Rebuild。Expected: `0 Error(s), 0 Warning(s)`。

- [x] **Step 6: 提交 PI 实现**

```powershell
git add firmware/SmartHood/Control `
        firmware/SmartHood/App/Src/app_tasks.c `
        firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
git commit -m "feat: add fixed-point PI controller"
```

### Task 4: 建立 PA0 到 MotorTask 的命令队列

**Files:**
- Modify: `firmware/SmartHood/App/Inc/app_tasks.h`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/Core/Src/freertos.c`

- [x] **Step 1: 声明队列初始化接口**

在`app_tasks.h`包含区增加：

```c
#include <stdbool.h>
```

在任务声明前增加：

```c
/**
 * @brief 在任务创建前建立电机控制命令队列。
 * @return 创建成功返回 true，失败返回 false。
 */
bool App_MotorControl_Init(void);
```

- [x] **Step 2: 增加单一 NEXT 命令队列**

在`app_tasks.c`私有定义区增加：

```c
/** 电机任务拥有状态，按键任务只发送“切换到下一状态”。 */
typedef enum
{
    APP_MOTOR_COMMAND_NEXT = 0
} App_MotorCommand_t;

/** 最多缓存 4 次按键命令，正常消抖操作不会填满。 */
#define APP_MOTOR_COMMAND_QUEUE_LENGTH 4U

static osMessageQueueId_t app_motor_command_queue = NULL;

bool App_MotorControl_Init(void)
{
    app_motor_command_queue = osMessageQueueNew(
        APP_MOTOR_COMMAND_QUEUE_LENGTH,
        sizeof(App_MotorCommand_t),
        NULL);

    return app_motor_command_queue != NULL;
}

static bool App_MotorPostNextCommand(void)
{
    App_MotorCommand_t command = APP_MOTOR_COMMAND_NEXT;

    if (app_motor_command_queue == NULL)
    {
        return false;
    }

    return osMessageQueuePut(app_motor_command_queue,
                             &command,
                             0U,
                             0U) == osOK;
}
```

- [x] **Step 3: 在任务创建前初始化队列**

在`freertos.c`的`MX_FREERTOS_Init()`中，紧跟`DebugLog_Init()`成功检查后增加：

```c
  /* 按键任务和电机任务启动前先创建控制命令队列。 */
  if (!App_MotorControl_Init())
  {
    Error_Handler();
  }
```

- [x] **Step 4: DefaultTask 不再直接控制 PWM**

从`app_tasks.c`删除：

```c
static const uint8_t app_motor_duty_levels[] = { ... };
#define APP_MOTOR_LEVEL_COUNT ...
static void App_ApplyMotorDuty(uint8_t duty_percent) { ... }
```

从`App_DefaultTask()`删除：

```c
uint8_t motor_level_index = 0U;
bool motor_ready;
motor_ready = BSP_Motor_Init();
```

以及对应的 M4 电机初始化日志。

将 PA0 有效按下后的原挡位处理替换为：

```c
if (!App_MotorPostNextCommand())
{
    DebugLog_Printf("motor command queue full\r\n");
}
```

- [x] **Step 5: Rebuild**

Expected: `0 Error(s), 0 Warning(s)`。此检查点的 M5 `App_MotorTask`仍只测速，因此接电机后按键暂时不会改变 PWM；不要进行硬件运行测试。

- [x] **Step 6: 提交任务所有权改造**

```powershell
git add firmware/SmartHood/App `
        firmware/SmartHood/Core/Src/freertos.c
git commit -m "refactor: route motor commands to MotorTask"
```

### Task 5: 将 MotorTask 扩展为闭环状态机

**Files:**
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/App/Inc/app_tasks.h`

- [x] **Step 1: 定义控制常量和状态**

用以下定义替换 M5 的 RPM 日志常量：

```c
#define APP_CONTROL_PERIOD_MS              50U
#define APP_CONTROL_LOG_SAMPLE_COUNT       10U
#define APP_MOTOR_START_DUTY_PERCENT       30U
#define APP_MOTOR_START_TIME_MS            300U
#define APP_MOTOR_LOW_TARGET_COUNT         130L
#define APP_MOTOR_HIGH_TARGET_COUNT        195L
#define APP_MOTOR_LOW_FEEDFORWARD          50L
#define APP_MOTOR_HIGH_FEEDFORWARD         70L
#define APP_MOTOR_OUTPUT_MIN               30L
#define APP_MOTOR_OUTPUT_MAX               90L
#define APP_ENCODER_ZERO_THRESHOLD         1L
#define APP_ENCODER_FAULT_SAMPLE_COUNT     10U
#define APP_CONTROL_KP_Q8                  64L
#define APP_CONTROL_KI_Q8                  4L
#define APP_CONTROL_INTEGRAL_MIN           (-2048L)
#define APP_CONTROL_INTEGRAL_MAX           2048L

typedef enum
{
    APP_MOTOR_STATE_STOP = 0,
    APP_MOTOR_STATE_LOW_START,
    APP_MOTOR_STATE_LOW_PI,
    APP_MOTOR_STATE_HIGH_START,
    APP_MOTOR_STATE_HIGH_PI,
    APP_MOTOR_STATE_FAULT
} App_MotorState_t;
```

- [x] **Step 2: 增加状态文本和目标选择辅助函数**

```c
static const char *App_MotorStateText(App_MotorState_t state)
{
    switch (state)
    {
        case APP_MOTOR_STATE_LOW_START:
            return "LOW_START";
        case APP_MOTOR_STATE_LOW_PI:
            return "LOW";
        case APP_MOTOR_STATE_HIGH_START:
            return "HIGH_START";
        case APP_MOTOR_STATE_HIGH_PI:
            return "HIGH";
        case APP_MOTOR_STATE_FAULT:
            return "FAULT";
        case APP_MOTOR_STATE_STOP:
        default:
            return "STOP";
    }
}

static int32_t App_MotorTargetCount(App_MotorState_t state)
{
    if ((state == APP_MOTOR_STATE_LOW_START) ||
        (state == APP_MOTOR_STATE_LOW_PI))
    {
        return APP_MOTOR_LOW_TARGET_COUNT;
    }

    if ((state == APP_MOTOR_STATE_HIGH_START) ||
        (state == APP_MOTOR_STATE_HIGH_PI))
    {
        return APP_MOTOR_HIGH_TARGET_COUNT;
    }

    return 0L;
}
```

- [x] **Step 3: 用闭环实现替换 App_MotorTask**

完整替换原`App_MotorTask()`：

```c
void App_MotorTask(void *argument)
{
    ControlPi_t controller;
    App_MotorState_t state = APP_MOTOR_STATE_STOP;
    uint32_t sample_tick;
    uint32_t state_enter_tick;
    uint32_t log_sample_count = 0U;
    uint32_t zero_sample_count = 0U;
    int32_t actual_sum = 0L;
    int32_t signed_delta_sum = 0L;
    uint8_t duty_percent = 0U;

    (void)argument;

#if APP_CONTROL_PI_SELF_TEST_ENABLED
    if (!ControlPi_RunSelfTests())
    {
        DebugLog_Printf("control PI self-test FAILED\r\n");
        BSP_Motor_Stop();
        for (;;)
        {
            osDelay(1000U);
        }
    }
    DebugLog_Printf("control PI self-test PASSED\r\n");
#endif

    ControlPi_Init(&controller,
                   APP_CONTROL_KP_Q8,
                   APP_CONTROL_KI_Q8,
                   APP_CONTROL_INTEGRAL_MIN,
                   APP_CONTROL_INTEGRAL_MAX,
                   APP_MOTOR_OUTPUT_MIN,
                   APP_MOTOR_OUTPUT_MAX);

    if (!BSP_Motor_Init())
    {
        DebugLog_Printf("motor init failed, state=STOP\r\n");
        for (;;)
        {
            BSP_Motor_Stop();
            osDelay(1000U);
        }
    }

    BSP_Encoder_Init();
    if (!BSP_Encoder_Start())
    {
        DebugLog_Printf("encoder start failed, state=STOP\r\n");
        for (;;)
        {
            BSP_Motor_Stop();
            osDelay(1000U);
        }
    }

    BSP_Motor_Stop();
    DebugLog_Printf("motor control ready, state=STOP\r\n");

    sample_tick = osKernelGetTickCount();
    state_enter_tick = sample_tick;

    for (;;)
    {
        App_MotorCommand_t command;
        Encoder_Direction_t direction;
        int16_t delta;
        int32_t delta_32;
        int32_t actual_count;
        int32_t target_count;
        int32_t feedforward;
        uint32_t now_tick;

        sample_tick += APP_CONTROL_PERIOD_MS;
        (void)osDelayUntil(sample_tick);
        now_tick = osKernelGetTickCount();

        if (osMessageQueueGet(app_motor_command_queue,
                              &command,
                              NULL,
                              0U) == osOK)
        {
            if (state == APP_MOTOR_STATE_FAULT)
            {
                state = APP_MOTOR_STATE_STOP;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
                duty_percent = 0U;
                DebugLog_Printf("motor fault cleared, state=STOP\r\n");
            }
            else if (state == APP_MOTOR_STATE_STOP)
            {
                state = APP_MOTOR_STATE_LOW_START;
                state_enter_tick = now_tick;
                ControlPi_Reset(&controller);
                BSP_Motor_SetDuty(APP_MOTOR_START_DUTY_PERCENT);
                duty_percent = APP_MOTOR_START_DUTY_PERCENT;
                DebugLog_Printf("motor state=LOW_START duty=30%%\r\n");
            }
            else if ((state == APP_MOTOR_STATE_LOW_START) ||
                     (state == APP_MOTOR_STATE_LOW_PI))
            {
                state = APP_MOTOR_STATE_HIGH_START;
                state_enter_tick = now_tick;
                ControlPi_Reset(&controller);
                BSP_Motor_SetDuty(APP_MOTOR_START_DUTY_PERCENT);
                duty_percent = APP_MOTOR_START_DUTY_PERCENT;
                DebugLog_Printf("motor state=HIGH_START duty=30%%\r\n");
            }
            else
            {
                state = APP_MOTOR_STATE_STOP;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
                duty_percent = 0U;
                DebugLog_Printf("motor state=STOP duty=0%%\r\n");
            }
        }

        delta = BSP_Encoder_ReadDelta(&direction);
        delta_32 = (int32_t)delta;
        actual_count = (delta_32 < 0L) ? -delta_32 : delta_32;
        target_count = App_MotorTargetCount(state);
        feedforward = 0L;

        if ((state == APP_MOTOR_STATE_LOW_START) ||
            (state == APP_MOTOR_STATE_HIGH_START))
        {
            BSP_Motor_SetDuty(APP_MOTOR_START_DUTY_PERCENT);
            duty_percent = APP_MOTOR_START_DUTY_PERCENT;

            if ((uint32_t)(now_tick - state_enter_tick) >=
                APP_MOTOR_START_TIME_MS)
            {
                state = (state == APP_MOTOR_STATE_LOW_START) ?
                    APP_MOTOR_STATE_LOW_PI :
                    APP_MOTOR_STATE_HIGH_PI;
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
            }
        }

        if ((state == APP_MOTOR_STATE_LOW_PI) ||
            (state == APP_MOTOR_STATE_HIGH_PI))
        {
            if (actual_count <= APP_ENCODER_ZERO_THRESHOLD)
            {
                zero_sample_count++;
            }
            else
            {
                zero_sample_count = 0U;
            }

            if (zero_sample_count >= APP_ENCODER_FAULT_SAMPLE_COUNT)
            {
                state = APP_MOTOR_STATE_FAULT;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                duty_percent = 0U;
                DebugLog_Printf(
                    "motor state=FAULT reason=ENCODER_TIMEOUT duty=0%%\r\n");
            }
            else
            {
                feedforward =
                    (state == APP_MOTOR_STATE_LOW_PI) ?
                    APP_MOTOR_LOW_FEEDFORWARD :
                    APP_MOTOR_HIGH_FEEDFORWARD;

                duty_percent = (uint8_t)ControlPi_Update(
                    &controller,
                    target_count,
                    actual_count,
                    feedforward);

                BSP_Motor_SetDuty(duty_percent);
            }
        }

        actual_sum += actual_count;
        signed_delta_sum += delta_32;
        log_sample_count++;

        if (log_sample_count >= APP_CONTROL_LOG_SAMPLE_COUNT)
        {
            int32_t actual_average;
            int32_t error_average;
            const char *direction_text;

            actual_average = actual_sum /
                (int32_t)APP_CONTROL_LOG_SAMPLE_COUNT;
            error_average = target_count - actual_average;

            if (signed_delta_sum > 0L)
            {
                direction_text = "forward";
            }
            else if (signed_delta_sum < 0L)
            {
                direction_text = "reverse";
            }
            else
            {
                direction_text = "stopped";
            }

            DebugLog_Printf(
                "control state=%s target=%ld actual=%ld "
                "error=%ld duty=%u integral=%ld fault=%u dir=%s\r\n",
                App_MotorStateText(state),
                (long)target_count,
                (long)actual_average,
                (long)error_average,
                (unsigned int)duty_percent,
                (long)ControlPi_GetIntegral(&controller),
                (unsigned int)(state == APP_MOTOR_STATE_FAULT),
                direction_text);

            actual_sum = 0L;
            signed_delta_sum = 0L;
            log_sample_count = 0U;
        }
    }
}
```

- [x] **Step 4: 更新任务注释**

将`app_tasks.h`和`app_tasks.c`中的 M5“只测速、不修改 PWM”注释改为：

```c
/**
 * @brief 电机控制任务：50 ms读取编码器，执行软启动、PI和故障停机。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
```

- [x] **Step 5: Rebuild**

Expected: `0 Error(s), 0 Warning(s)`；`app_tasks.c`、`control_pi.c`和`control_pi_selftest.c`均参与编译。

- [x] **Step 6: 提交闭环状态机**

```powershell
git add firmware/SmartHood/App
git commit -m "feat: add M6 relative speed control state machine"
```

### Task 6: 无电机闭环与故障锁存验证

**Files:**
- Modify: `docs/test-records.md`

- [ ] **Step 1: 断电并隔离电机输出**

断开电机 AO1/AO2 或断开 TB6612 VM。STM32、ST-Link、USB 转 TTL 可正常连接。不要在通电状态插拔电机线。

- [ ] **Step 2: 烧录并检查上电停止**

Expected:

```text
motor control ready, state=STOP
control state=STOP target=0 actual=0 duty=0 fault=0
```

PA1 心跳、DHT11 和 TFT 回归功能继续正常。

- [ ] **Step 3: 按 PA0 触发无反馈故障**

第一次短按后应先出现`LOW_START duty=30%`，约 300 ms 后进入 LOW；再经过约 500 ms 无计数后出现：

```text
motor state=FAULT reason=ENCODER_TIMEOUT duty=0%
```

- [ ] **Step 4: 验证故障清除不自动重启**

FAULT 状态按一次 PA0。

Expected:

```text
motor fault cleared, state=STOP
```

此时必须保持停止。第二次按 PA0 才再次进入`LOW_START`。

- [ ] **Step 5: 记录并提交**

```powershell
git add docs/test-records.md
git commit -m "test: verify M6 no-feedback fault handling"
```

### Task 7: 低档闭环空载调参

**Files:**
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `docs/test-records.md`

- [ ] **Step 1: 断电恢复电机和编码器接线**

保持原 M5/M4 接线：5V VM、3.3V VCC、共地、PC6=C1、PC7=C2、AO1/AO2接电机。禁止接扇叶或机械负载。

- [ ] **Step 2: 上电确认 STOP**

观察 3 秒。电机不得自行转动，无异常气味、明显发热或系统重启。

- [ ] **Step 3: 短按进入低档**

Expected:

```text
LOW_START duty=30
LOW target=130 actual=... duty=... fault=0
```

连续观察 10 秒。验收范围：`actual=117～143`，PWM 不超过 90%，没有持续振荡。

- [ ] **Step 4: 按规则单变量调参**

仅在需要时调整：

- 实际计数持续上下摆动超过目标的±10%：`Kp`从64降到48；仍摆动时`Ki`从4降到2。
- 响应平稳但3秒后仍有固定偏差：`Ki`从4升到6，最多升到8。
- 响应过慢且无振荡：`Kp`从64升到80，最多升到96。
- PWM连续1秒保持90%仍达不到目标：立即停止，不提高90%上限，记录“目标在当前供电下不可达”。

每次只修改一个参数，Rebuild、烧录并重新观察 10 秒。

- [ ] **Step 5: 验证停止与 RST**

再按两次 PA0，经过高档启动后进入 STOP；随后重新进入低档并按 RST。两种停止方式均应使电机立即停止。

- [ ] **Step 6: 记录并提交最终低档参数**

在`docs/test-records.md`记录 Kp、Ki、平均 actual、PWM 范围和现象。

```powershell
git add firmware/SmartHood/App/Src/app_tasks.c docs/test-records.md
git commit -m "test: tune M6 low relative speed target"
```

### Task 8: 高档阶跃与编码器故障验收

**Files:**
- Modify: `docs/test-records.md`
- Modify: `docs/project-guide.md`

- [ ] **Step 1: 低档稳定后切换高档**

低档稳定运行 5 秒后短按 PA0。

Expected: 先进入`HIGH_START`，随后进入`HIGH`；`actual`最终保持在`175～215`，PWM 不超过90%，无持续振荡。

- [ ] **Step 2: 验证高档停止**

高档稳定后短按 PA0。

Expected: `state=STOP duty=0`，电机停止。

- [ ] **Step 3: 安全验证编码器断线**

断电，断开 C1/C2，重新上电并启动低档。不要在电机运行中拔线。

Expected: 启动阶段后约500 ms进入`ENCODER_TIMEOUT`，PWM归零并锁存 FAULT。

- [ ] **Step 4: 恢复接线并回归**

断电恢复 C1/C2，再上电验证 STOP、低档、高档、停止、心跳、DHT11、TFT 和串口日志。

- [ ] **Step 5: 更新阶段结论**

`docs/test-records.md`记录相对闭环、阶跃、FAULT和未执行项目；`docs/project-guide.md`把 M6 标为“相对闭环功能通过”，同时继续声明未标定 CPR、未做堵转和带负载测试。

- [ ] **Step 6: 提交验收结果**

```powershell
git add docs/project-guide.md docs/test-records.md
git commit -m "test: validate M6 relative PI control"
```

### Task 9: 最终检查、合并与远端同步

**Files:**
- Verify: all M6 files
- Verify: `docs/superpowers/specs/2026-08-15-m6-relative-pi-control-design.md`
- Verify: `docs/superpowers/plans/2026-08-15-m6-relative-pi-control.md`

- [ ] **Step 1: 检查范围边界**

确认没有把相对计数写成精确 RPM，没有实现自动模式、MQ-2融合、LVGL、软件反转或未测试的堵转保护。

- [ ] **Step 2: 最终 Rebuild 与 HEX**

Rebuild。Expected: `0 Error(s), 0 Warning(s)`。记录程序尺寸、构建时间和 SHA-256。

- [ ] **Step 3: 检查差异**

```powershell
git diff --check
git status --short
git log --oneline main..HEAD
```

Expected: 无空白错误；只包含计划内 Control、App、FreeRTOS、Keil 工程和文档修改。

- [ ] **Step 4: 最终提交**

```powershell
git add docs firmware
git commit -m "feat: complete M6 relative PI control"
```

- [ ] **Step 5: 按分支收尾流程处理**

在最终构建和硬件验收通过后，选择本地合并、创建 PR 或保留分支。GitHub 网络失败时保留本地提交并明确记录领先状态，不把本地成功描述为远端成功。
