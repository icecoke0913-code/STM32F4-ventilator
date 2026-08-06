# M4 TB6612 Open-Loop Motor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有SmartHood固件中加入TB6612FNG A通道开环电机控制，使PA0每次有效短按按“停止→30%→50%→70%→停止”循环，并保证上电、复位和初始化失败时电机始终安全停止。

**Architecture:** CubeMX生成TIM4_CH1的20kHz PWM和三个安全初始为低的控制GPIO；独立`bsp_motor`模块封装TB6612方向、待机和占空比操作；现有`App_DefaultTask`改为20ms轮询以完成PA0消抖与按下沿识别，同时使用独立1秒节拍保持PA1心跳和USART1日志。M4不新增MotorTask、不连接编码器、不实现PID或软件反转。

**Tech Stack:** STM32F407VET6、STM32CubeMX 6.16.1、STM32CubeF4 1.28.3、HAL、FreeRTOS CMSIS_V2、Keil MDK-ARM 5/ARMCC 5.06、TIM4 PWM、GPIO、TB6612FNG、JGA12-N20-50B。

---

## 文件结构与职责

```text
firmware/SmartHood/SmartHood.ioc
    CubeMX配置源：PB6/TIM4_CH1、PB7、PB8、PB9及安全初始电平。

firmware/SmartHood/Core/Inc/main.h
firmware/SmartHood/Core/Inc/tim.h
firmware/SmartHood/Core/Src/gpio.c
firmware/SmartHood/Core/Src/tim.c
firmware/SmartHood/Core/Src/main.c
firmware/SmartHood/Core/Src/stm32f4xx_hal_msp.c
    CubeMX生成的GPIO标签、TIM4句柄、PWM初始化和PB6复用配置。

firmware/SmartHood/BSP/Inc/bsp_motor.h
firmware/SmartHood/BSP/Src/bsp_motor.c
    TB6612 A通道安全初始化、固定正转、PWM占空比换算和停止接口。

firmware/SmartHood/App/Src/app_tasks.c
    初始化电机BSP；以20ms周期处理PA0消抖和按下沿；循环四个挡位；保持1秒心跳。

firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
    Keil BSP分组加入bsp_motor.c；CubeMX维护生成文件。

docs/project-guide.md
docs/hardware-connections.md
docs/test-records.md
    记录M4配置、接线、构建、空载测试、限制和最终验收。
```

本项目没有用于模拟TIM PWM和TB6612输出级的主机单元测试框架，也不为HAL编写只验证模拟调用次数的测试。验证按风险分层进行：基线构建、CubeMX空外设构建、BSP未调用构建、完整链接、无电机烧录、断电接线、空载挡位测试、安全复位测试和M1/M2/M3A回归。

---

### Task 1: 建立M4开发检查点

**Files:**
- Inspect: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`
- Inspect: `firmware/SmartHood/SmartHood.ioc`
- Inspect: `docs/superpowers/specs/2026-08-03-m4-tb6612-open-loop-motor-design.md`

- [x] **Step 1: 由助手检查并同步Git基线**

应确认：

```text
当前分支：main
工作区：干净
本地至少包含设计提交：4d1e1ab
```

当前本地因GitHub连接超时而领先`origin/main`。助手先重试推送设计与计划提交；网络仍不可用时明确保留本地领先状态，不要求用户执行Git命令，也不阻止本地教学。

- [x] **Step 2: 由助手创建M4功能分支**

分支名：

```text
codex/feature-m4-open-loop-motor
```

所有分支、暂存、提交、推送和最终合并操作由助手完成。

- [x] **Step 3: 用户执行M3A基线全量编译**

在Keil中打开：

```text
firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
```

执行：

```text
Project → Rebuild all target files
```

预期基线：

```text
Code=24002
RO-data=1286
RW-data=156
ZI-data=39524
0 Error(s), 0 Warning(s)
```

用户把完整构建摘要发给助手。若大小有变化但错误和警告为0，先检查是否打开了不同Target或工程；不得在基线异常时继续配置TIM4。

---

### Task 2: 在CubeMX配置TIM4_CH1和TB6612控制GPIO

**Files:**
- Modify: `firmware/SmartHood/SmartHood.ioc`
- Generate: `firmware/SmartHood/Core/Inc/main.h`
- Generate: `firmware/SmartHood/Core/Inc/tim.h`
- Generate: `firmware/SmartHood/Core/Src/gpio.c`
- Generate: `firmware/SmartHood/Core/Src/tim.c`
- Generate: `firmware/SmartHood/Core/Src/main.c`
- Generate: `firmware/SmartHood/Core/Src/stm32f4xx_hal_msp.c`

- [x] **Step 1: 打开现有IOC并确认时钟**

打开：

```text
firmware/SmartHood/SmartHood.ioc
```

在Clock Configuration确认：

```text
SYSCLK = 168 MHz
PCLK1 = 42 MHz
APB1 Timer clocks = 84 MHz
```

TIM4挂在APB1。APB1分频不为1时，定时器时钟为PCLK1的2倍，因此这里使用84MHz计算PWM。

- [x] **Step 2: 配置PB6为TIM4_CH1 PWM**

在Pinout视图单击PB6，选择：

```text
TIM4_CH1
```

在Timers → TIM4中设置：

```text
Clock Source：Internal Clock（若当前CubeMX界面不单独显示该项，以PWM Generation CH1正常启用且无引脚冲突为准）
Channel 1：PWM Generation CH1
Prescaler：0
Counter Mode：Up
Counter Period：4199
Internal Clock Division：No Division
Auto-reload preload：Disable
Pulse：0
OC Mode：PWM mode 1
OC Polarity：High
OC Fast Mode：Disable
```

计算关系：

```text
PWM = 84,000,000 / (Prescaler + 1) / (Period + 1)
    = 84,000,000 / 1 / 4200
    = 20,000 Hz
```

初始Pulse必须为0，使PWM启动后仍为0%占空比。

- [x] **Step 3: 配置PB7、PB8和PB9为安全GPIO输出**

分别设置：

```text
PB7 → GPIO_Output，User Label：MOTOR_AIN1
PB8 → GPIO_Output，User Label：MOTOR_AIN2
PB9 → GPIO_Output，User Label：MOTOR_STBY
```

在System Core → GPIO中，三个引脚均设置：

```text
GPIO output level：Low
GPIO mode：Output Push Pull
GPIO Pull-up/Pull-down：No pull-up and no pull-down
Maximum output speed：Low
```

三个初始电平全部为Low是硬性安全要求。禁止把STBY初始值设为High。

- [x] **Step 4: 截图核对后再生成代码**

用户发送以下截图供助手核对：

```text
TIM4 Parameter Settings完整页面
PB6/PB7/PB8/PB9 Pinout区域
GPIO中PB7/PB8/PB9三行配置
Clock Configuration中的APB1 Timer clocks
```

截图确认前不生成代码，不连接TB6612、电机或9V适配器。

---

### Task 3: 生成CubeMX代码并验证空外设工程

**Files:**
- Modify: `firmware/SmartHood/SmartHood.ioc`
- Verify generated: `firmware/SmartHood/Core/Inc/main.h`
- Verify generated: `firmware/SmartHood/Core/Inc/tim.h`
- Verify generated: `firmware/SmartHood/Core/Src/gpio.c`
- Verify generated: `firmware/SmartHood/Core/Src/tim.c`
- Verify generated: `firmware/SmartHood/Core/Src/main.c`
- Verify generated: `firmware/SmartHood/Core/Src/stm32f4xx_hal_msp.c`

- [x] **Step 1: 生成代码**

在CubeMX执行：

```text
GENERATE CODE
```

保持已有选项：

```text
Keep User Code when re-generating：启用
Generate peripheral initialization as pair of .c/.h：启用
Toolchain：MDK-ARM
```

- [x] **Step 2: 由助手检查生成保护点**

助手检查以下内容没有被覆盖：

```text
freertos.c仍包含app_tasks.h和debug_log.h
StartDefaultTask仍调用App_DefaultTask(argument)
StartSensorTask仍调用App_SensorTask(argument)
MX_FREERTOS_Init()仍调用DebugLog_Init()
app_tasks.c、bsp_dht11.c和bsp_st7735s.c内容未被CubeMX改写
```

- [x] **Step 3: 核对TIM4生成结果**

`tim.h/.c`和`main.c`应体现等价配置：

```c
extern TIM_HandleTypeDef htim4;

htim4.Instance = TIM4;
htim4.Init.Prescaler = 0;
htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
htim4.Init.Period = 4199;
htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

sConfigOC.OCMode = TIM_OCMODE_PWM1;
sConfigOC.Pulse = 0;
sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
```

PB6应在TIM4 PostInit中配置为`GPIO_AF2_TIM4`。`main.c`应在`MX_GPIO_Init()`之后调用`MX_TIM4_Init()`，具体相邻顺序由CubeMX决定。

- [x] **Step 4: 核对GPIO标签和初始低电平**

`main.h`应生成：

```c
#define MOTOR_AIN1_Pin GPIO_PIN_7
#define MOTOR_AIN1_GPIO_Port GPIOB
#define MOTOR_AIN2_Pin GPIO_PIN_8
#define MOTOR_AIN2_GPIO_Port GPIOB
#define MOTOR_STBY_Pin GPIO_PIN_9
#define MOTOR_STBY_GPIO_Port GPIOB
```

`gpio.c`应在配置输出模式前先执行等价的低电平写入：

```c
HAL_GPIO_WritePin(GPIOB,
                  MOTOR_AIN1_Pin | MOTOR_AIN2_Pin | MOTOR_STBY_Pin,
                  GPIO_PIN_RESET);
```

- [x] **Step 5: 用户执行空外设Rebuild**

此时TB6612、电机和9V适配器保持未连接。执行Rebuild，预期：

```text
compiling tim.c...
compiling gpio.c...
compiling stm32f4xx_hal_tim.c...
0 Error(s), 0 Warning(s)
```

程序大小会因TIM4 PWM初始化略有增加，不预设精确字节数。

- [x] **Step 6: 由助手提交CubeMX检查点**

提交信息：

```text
feat: configure TIM4 motor PWM outputs
```

---

### Task 4: 创建电机BSP公共接口

**Files:**
- Create: `firmware/SmartHood/BSP/Inc/bsp_motor.h`

- [x] **Step 1: 创建bsp_motor.h**

创建文件并录入完整内容：

```c
/**
 * @file bsp_motor.h
 * @brief TB6612FNG A通道开环电机控制公共接口。
 */

#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化TIM4 PWM，并使TB6612保持安全停止状态。
 * @return PWM成功启动时返回true，失败时返回false且STBY保持低电平。
 */
bool BSP_Motor_Init(void);

/**
 * @brief 设置A通道固定正转占空比。
 * @param duty_percent 百分比占空比；大于100时在内部限制为100。
 *
 * 传入0会执行完整安全停机。驱动尚未成功初始化时，
 * 任何非零请求也只会保持停止，不会使能TB6612。
 */
void BSP_Motor_SetDuty(uint8_t duty_percent);

/**
 * @brief 立即关闭TB6612输出、清零PWM并拉低方向引脚。
 */
void BSP_Motor_Stop(void);

#endif
```

- [x] **Step 2: 检查接口设计**

确认：

```text
只有Init、SetDuty、Stop三个公共接口
没有反转接口
没有编码器或RPM参数
所有公共接口均有中文用途、参数和安全行为说明
```

- [x] **Step 3: 头文件语法编译**

暂时在`app_tasks.c`包含区加入：

```c
#include "bsp_motor.h"
```

此时不要调用接口。执行Build，预期：

```text
0 Error(s), 0 Warning(s)
```

新增声明不会产生未定义符号，编译用于确认UTF-8注释、`stdbool.h`和`stdint.h`与ARMCC 5兼容。

---

### Task 5: 实现TB6612 A通道BSP

**Files:**
- Create: `firmware/SmartHood/BSP/Src/bsp_motor.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [x] **Step 1: 创建bsp_motor.c**

创建文件并录入完整内容：

```c
/**
 * @file bsp_motor.c
 * @brief TB6612FNG A通道安全初始化、固定正转和PWM调速实现。
 */

#include "bsp_motor.h"

#include "main.h"
#include "tim.h"

/** 记录PWM是否已成功启动，防止初始化失败后误使能驱动器。 */
static bool motor_initialized = false;

/**
 * @brief 将百分比换算为TIM4_CH1比较值。
 * @param duty_percent 已限制在1～100范围内的占空比。
 * @return 对应一个PWM周期的高电平计数值。
 */
static uint32_t Motor_DutyToPulse(uint8_t duty_percent)
{
    uint32_t period_counts;

    /* ARR=4199时一个周期共有4200个计数，不能直接使用ARR乘百分比。 */
    period_counts = __HAL_TIM_GET_AUTORELOAD(&htim4) + 1U;

    return (period_counts * (uint32_t)duty_percent) / 100U;
}

bool BSP_Motor_Init(void)
{
    /* 初始化入口先进入安全态，避免上一次调试状态影响本次启动。 */
    motor_initialized = false;
    BSP_Motor_Stop();

    if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)
    {
        BSP_Motor_Stop();
        return false;
    }

    motor_initialized = true;
    return true;
}

void BSP_Motor_SetDuty(uint8_t duty_percent)
{
    uint32_t pulse;

    if ((!motor_initialized) || (duty_percent == 0U))
    {
        BSP_Motor_Stop();
        return;
    }

    if (duty_percent > 100U)
    {
        duty_percent = 100U;
    }

    pulse = Motor_DutyToPulse(duty_percent);

    /*
     * 调整运行状态时先保持STBY低，再准备方向和PWM，
     * 最后才使能输出，避免切换过程中出现短暂错误驱动。
     */
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port,
                      MOTOR_STBY_Pin,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port,
                      MOTOR_AIN1_Pin,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port,
                      MOTOR_AIN2_Pin,
                      GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse);
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port,
                      MOTOR_STBY_Pin,
                      GPIO_PIN_SET);
}

void BSP_Motor_Stop(void)
{
    /* 先关闭驱动输出，再清PWM和方向，形成确定的高阻停止状态。 */
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port,
                      MOTOR_STBY_Pin,
                      GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0U);
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port,
                      MOTOR_AIN1_Pin,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port,
                      MOTOR_AIN2_Pin,
                      GPIO_PIN_RESET);
}
```

- [x] **Step 2: 人工核对安全顺序和换算**

检查：

```text
Init开始时先Stop
PWM启动失败时再次Stop并返回false
未初始化时非零占空比不会拉高STBY
0%等价于完整Stop
运行顺序为STBY低→方向/PWM准备→STBY高
停止顺序为STBY低→CCR清零→方向低
ARR=4199时30%/50%/70%分别换算为1260/2100/2940
```

- [x] **Step 3: 将bsp_motor.c加入Keil BSP分组**

在Keil执行：

```text
右键BSP组
→ Add Existing Files to Group 'BSP'...
→ 选择 ..\BSP\Src\bsp_motor.c
→ 文件类型选择C Source file
```

BSP组应至少包含：

```text
fonts.c
bsp_st7735s.c
bsp_dht11.c
bsp_motor.c
```

- [x] **Step 4: 编译尚未调用的BSP**

执行Rebuild，预期：

```text
compiling bsp_motor.c...
0 Error(s), 0 Warning(s)
```

应用层尚未调用时，链接器可能移除电机BSP，因此程序大小不是本检查点标准。

- [x] **Step 5: 由助手提交BSP检查点**

提交信息：

```text
feat: add TB6612 motor BSP
```

---

### Task 6: 在默认任务中接入消抖和四挡循环

**Files:**
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`

- [x] **Step 1: 确认头文件包含区**

`app_tasks.c`头部应包含：

```c
#include "app_tasks.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp_dht11.h"
#include "bsp_motor.h"
#include "bsp_st7735s.h"
#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"
```

显式包含`stdbool.h`，避免应用层的`bool`依赖其他BSP头文件间接提供。

- [x] **Step 2: 在包含区之后加入控制参数和挡位表**

```c
/** 默认任务快速循环周期，用于可靠识别短按。 */
#define APP_MAIN_LOOP_PERIOD_MS       20U

/** 原始按键保持不变达到此时间后，才更新稳定状态。 */
#define APP_KEY_DEBOUNCE_MS           40U

/** PA1翻转和心跳日志保持原有1秒周期。 */
#define APP_HEARTBEAT_PERIOD_MS     1000U

/** M4固定正转挡位，占空比按短按顺序循环。 */
static const uint8_t app_motor_duty_levels[] =
{
    0U,
    30U,
    50U,
    70U
};

#define APP_MOTOR_LEVEL_COUNT \
    ((uint8_t)(sizeof(app_motor_duty_levels) / \
               sizeof(app_motor_duty_levels[0])))
```

- [x] **Step 3: 在App_RunDisplayTest之前加入挡位应用函数**

```c
/**
 * @brief 应用一个已确认的M4电机挡位并输出状态日志。
 * @param duty_percent 允许值为0、30、50或70。
 */
static void App_ApplyMotorDuty(uint8_t duty_percent)
{
    if (duty_percent == 0U)
    {
        BSP_Motor_Stop();
        DebugLog_Printf("motor duty=0%%, state=STOP\r\n");
    }
    else
    {
        BSP_Motor_SetDuty(duty_percent);
        DebugLog_Printf("motor duty=%u%%, state=RUN\r\n",
                        (unsigned int)duty_percent);
    }
}
```

- [x] **Step 4: 用以下完整实现替换App_DefaultTask**

只替换`App_DefaultTask()`函数，不修改`App_RunDisplayTest()`和`App_SensorTask()`：

```c
/**
 * @brief 运行显示自检、心跳和M4单键电机挡位控制。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 快速循环每20ms采样PA0并执行40ms消抖；只有稳定状态从低变高时
 * 才切换一次挡位。PA1和心跳使用独立1秒节拍，避免快速循环改变M1行为。
 */
void App_DefaultTask(void *argument)
{
    uint32_t heartbeat = 0U;
    uint32_t heartbeat_tick;
    uint32_t candidate_since_tick;
    uint8_t motor_level_index = 0U;
    GPIO_PinState candidate_key_state;
    GPIO_PinState stable_key_state;
    bool motor_ready;

    (void)argument;

    DebugLog_Printf("\r\nSmartHood M1 start\r\n");

    motor_ready = BSP_Motor_Init();
    if (motor_ready)
    {
        DebugLog_Printf("motor init ok, state=STOP\r\n");
    }
    else
    {
        DebugLog_Printf("motor init failed, state=STOP\r\n");
    }

    if (App_RunDisplayTest())
    {
        DebugLog_Printf("ST7735S init and test OK\r\n");
    }
    else
    {
        /* 关闭背光，使显示初始化或绘图失败具有明确的可见状态。 */
        BSP_ST7735S_SetBacklight(false);
        DebugLog_Printf("ST7735S init or draw failed\r\n");
    }

    /*
     * 以任务开始处理按键时的实际电平作为初始稳定状态。
     * 若上电时PA0已被按住，不会因此触发电机；必须释放后再次按下。
     */
    stable_key_state = HAL_GPIO_ReadPin(USER_KEY_GPIO_Port,
                                        USER_KEY_Pin);
    candidate_key_state = stable_key_state;
    candidate_since_tick = HAL_GetTick();
    heartbeat_tick = candidate_since_tick;

    for (;;)
    {
        GPIO_PinState raw_key_state;
        uint32_t now_tick;

        now_tick = HAL_GetTick();
        raw_key_state = HAL_GPIO_ReadPin(USER_KEY_GPIO_Port,
                                        USER_KEY_Pin);

        if (raw_key_state != candidate_key_state)
        {
            /* 原始电平发生变化，重新开始消抖计时。 */
            candidate_key_state = raw_key_state;
            candidate_since_tick = now_tick;
        }
        else if ((candidate_key_state != stable_key_state) &&
                 ((uint32_t)(now_tick - candidate_since_tick) >=
                  APP_KEY_DEBOUNCE_MS))
        {
            stable_key_state = candidate_key_state;

            if (stable_key_state == GPIO_PIN_SET)
            {
                /* 只在稳定低到高的按下沿切换一次挡位。 */
                if (motor_ready)
                {
                    motor_level_index++;
                    if (motor_level_index >= APP_MOTOR_LEVEL_COUNT)
                    {
                        motor_level_index = 0U;
                    }

                    App_ApplyMotorDuty(
                        app_motor_duty_levels[motor_level_index]);
                }
                else
                {
                    BSP_Motor_Stop();
                    DebugLog_Printf(
                        "motor unavailable, state=STOP\r\n");
                }
            }
        }

        if ((uint32_t)(now_tick - heartbeat_tick) >=
            APP_HEARTBEAT_PERIOD_MS)
        {
            heartbeat_tick = now_tick;
            HAL_GPIO_TogglePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin);

            DebugLog_Printf("heartbeat=%lu key=%u tick=%lu\r\n",
                            (unsigned long)heartbeat,
                            (unsigned int)stable_key_state,
                            (unsigned long)now_tick);

            heartbeat++;
        }

        osDelay(APP_MAIN_LOOP_PERIOD_MS);
    }
}
```

- [x] **Step 5: 检查按键和安全逻辑**

逐项确认：

```text
任务循环为20ms，但PA1和heartbeat仍约1秒一次
原始电平稳定40ms后才更新按键状态
只有GPIO_PIN_RESET→GPIO_PIN_SET触发挡位切换
按住不会连续切挡
上电时按住PA0不会启动电机
motor_ready=false时每次按下都只Stop
挡位索引只产生0、1、2、3，不会越界
首次有效按下从索引0切换到30%
```

- [x] **Step 6: 全量编译验证完整链接**

执行Rebuild，预期日志包括：

```text
compiling bsp_motor.c...
compiling app_tasks.c...
compiling tim.c...
linking...
0 Error(s), 0 Warning(s)
```

Code应比M3A最终值增加，证明电机BSP和消抖逻辑进入最终镜像；不预设精确字节数。

- [x] **Step 7: 由助手提交应用集成检查点**

提交信息：

```text
feat: add PA0 motor duty control
```

---

### Task 7: 在不连接TB6612和电机时验证固件安全行为

**Files:**
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood.hex`
- Update after test: `docs/test-records.md`

- [x] **Step 1: 保持电机系统完全断开**

以下均不连接：

```text
9V适配器
DC-DC模块输出
TB6612
电机M1/M2
编码器VCC/C1/C2/GND
```

保留已经验证的STM32、ST-Link、USB转TTL、TFT和DHT11连接。

- [x] **Step 2: 烧录并观察启动日志**

预期出现：

```text
SmartHood M1 start
motor init ok, state=STOP
ST7735S init and test OK
```

`motor init ok`只表示TIM4 PWM成功启动，不表示TB6612硬件已连接。

- [x] **Step 3: 验证PA0消抖和挡位日志**

连续短按PA0，预期日志循环：

```text
motor duty=30%, state=RUN
motor duty=50%, state=RUN
motor duty=70%, state=RUN
motor duty=0%, state=STOP
```

验证：

```text
一次短按只产生一条motor日志
按住PA0至少2秒不连续切挡
释放后再次短按才进入下一挡
快速但明确的短按能够被识别
```

- [x] **Step 4: 验证M1/M2/M3A回归**

至少观察30秒：

```text
heartbeat持续递增且约1秒一次
PA1约1秒翻转一次
heartbeat中的key在按住时为1、释放时为0
TFT保持M2测试画面
DHT11每约2秒正常采集
无重复启动、任务卡死或日志乱码
```

- [x] **Step 5: 复位安全验证**

先通过PA0把软件挡位切到70%，再按RST。由于TB6612尚未连接，本步骤只验证软件状态：

```text
重启日志重新显示motor init ok, state=STOP
不会自动输出30%/50%/70%运行日志
再次短按PA0后从30%开始
```

---

### Task 8: 断电连接TB6612、电源和电机

**Files:**
- Verify: `docs/hardware-connections.md`

- [ ] **Step 1: 彻底断开全部电源**

移动任何电机相关导线前，断开：

```text
9V适配器
STM32 USB-C
ST-Link USB
USB转TTL
```

确认开发板电源灯、TFT背光和DC-DC指示灯均熄灭。

- [ ] **Step 2: 先连接控制和共地**

```text
STM32 PB6 → TB6612 PWMA
STM32 PB7 → TB6612 AIN1
STM32 PB8 → TB6612 AIN2
STM32 PB9 → TB6612 STBY
STM32 3.3V → TB6612 VCC
STM32 GND → TB6612 GND
DC-DC GND → 同一系统GND
```

TB6612任意一个明确标注的GND可作为模块地，但系统必须形成共同参考地。

- [ ] **Step 3: 连接电机供电路径**

```text
9V / 0.6A适配器 → DC-DC输入
DC-DC明确标注的5V输出 → TB6612 VM
```

禁止使用VIN或9V直通口作为VM。由于没有万用表，本步骤必须再次依据模块丝印和商家资料核对5V端子；有任何丝印不清时先拍照，不上电。

- [ ] **Step 4: 连接电机两根动力线**

```text
黑色M1 → TB6612 AO1
绿色M2 → TB6612 AO2
```

以下编码器线全部不接，并分别绝缘：

```text
橙色VCC
黄色C2
白色C1
红色GND
```

- [ ] **Step 5: 上电前逐点复核**

用户发送完整接线照片，由助手按以下清单核对：

```text
VM确实来自DC-DC 5V而不是VIN
VCC确实来自STM32 3.3V
三方GND共地
PB6/PB7/PB8/PB9没有接错位
M1/M2只接AO1/AO2
编码器四线均悬空绝缘
TB6612 B通道未使用
没有裸铜互碰或松动导线
电机已固定且未安装扇叶和机械负载
```

照片核对通过前不接通9V适配器。

---

### Task 9: 空载验证停止、三挡速度和复位安全

**Files:**
- Update: `docs/test-records.md`

- [ ] **Step 1: 先给STM32上电，保持9V断开**

接通STM32、调试器和串口，观察启动日志正常。此时电机电源VM没有供电，电机不得旋转。

- [ ] **Step 2: 在软件停止状态接通9V适配器**

确认最后状态为：

```text
motor init ok, state=STOP
```

再接通9V适配器。预期：

```text
电机不自动旋转
没有异味、冒烟或异常声音
TB6612和电机没有迅速明显发热
```

若上电立即旋转，立即断开9V适配器，不按PA0，记录现象并检查STBY、AIN1、AIN2和PWMA接线及生成初始电平。

- [ ] **Step 3: 短时测试30%挡位**

短按PA0一次，预期：

```text
motor duty=30%, state=RUN
电机固定方向低速旋转
```

只观察数秒。若不能可靠起转，不立即提高到100%或用手拨动；先停止并记录，随后检查供电和接线。

- [ ] **Step 4: 测试50%和70%速度趋势**

继续每次短按一次：

```text
30% → 50% → 70%
```

预期空载速度逐级上升。由于VM为5V、低于电机6V额定电压，不能要求达到标称300 RPM，只验收相对速度趋势。

- [ ] **Step 5: 从70%切换到停止**

再短按一次，预期：

```text
motor duty=0%, state=STOP
电机逐渐停下
```

直流电机和齿轮箱具有惯性，停止不是机械瞬时抱死；本设计使用STBY高阻停止，不测试短刹车模式。

- [ ] **Step 6: 验证长按不跳挡**

从停止状态短按进入30%，随后持续按住PA0至少2秒。预期：

```text
只产生一次30%日志
保持30%挡位
松开不会切挡
再次短按才进入50%
```

- [ ] **Step 7: 验证RST安全停止**

将挡位切到50%或70%，按下RST，预期：

```text
复位期间电机停止
重启后保持停止
启动日志重新显示motor init ok, state=STOP
不会恢复复位前挡位
```

若RST期间电机继续有明显驱动力，立即断开9V并检查PB9是否确实为MOTOR_STBY、初始Low。

- [ ] **Step 8: 验证物理方向处理原则**

若当前旋转方向不符合最终安装需求：

```text
停止电机
断开9V、USB-C、ST-Link和USB转TTL
确认所有电源灯熄灭
交换TB6612 AO1/AO2上的黑色M1和绿色M2
重新上电复测
```

不修改AIN1/AIN2软件逻辑，不增加反转测试。

- [ ] **Step 9: M1/M2/M3A最终回归**

电机在停止和运行状态下分别观察：

```text
USART1日志无乱码或明显交叉
heartbeat持续递增
PA1约1秒翻转
TFT画面稳定，无白屏或随机闪烁
DHT11继续输出OK
DHT11断开DATA后TIMEOUT，接回后无需复位恢复OK
无异常复位、任务卡死、异味或明显发热
```

M4不做堵转、带扇叶、带机械负载或长时间运行测试。

---

### Task 10: 更新项目文档并完成M4 Git收尾

**Files:**
- Update: `docs/project-guide.md`
- Update: `docs/hardware-connections.md`
- Update: `docs/test-records.md`
- Verify: all M4 firmware files

- [ ] **Step 1: 用户提供最终Rebuild摘要**

必须满足：

```text
0 Error(s), 0 Warning(s)
构建日志包含bsp_motor.c、app_tasks.c和tim.c
```

记录最终Code、RO-data、RW-data、ZI-data和Build Time。

- [ ] **Step 2: 由助手记录M4-T1结果**

`docs/test-records.md`至少记录：

```text
TIM4：84MHz输入、PSC=0、ARR=4199、20kHz、Pulse=0
PB7/PB8/PB9：推挽输出、No Pull、Low Speed、初始Low
实际供电：9V适配器→DC-DC→5V VM，STM32 3.3V→VCC，三方共地
实际电机线：黑M1→AO1，绿M2→AO2
上电不自动转结果
30%/50%/70%相对速度趋势
停止、长按、RST安全结果
电机和TB6612温度/气味/声音观察
M1/M2/M3A回归结果
没有万用表、未测电压跌落、未堵转和未带负载等限制
```

- [ ] **Step 3: 更新项目阶段状态**

若全部验收通过：

```text
project-guide.md：M4状态改为通过，下一阶段为M5编码器设计
hardware-connections.md：PB6/PB7/PB8/PB9状态改为已接线验证
test-records.md：M4-T1改为通过
```

若某一验收未通过，只记录实际现象和当前阻塞，不将M4标为完成。

- [ ] **Step 4: 由助手执行最终一致性检查**

检查：

```text
IOC与tim.c/gpio.c一致
Keil工程包含bsp_motor.c
没有未跟踪的M4源码
所有新增自编代码都有结构化中文注释
设计、计划、接线和测试记录使用同一套引脚与线色
没有把编码器、PID、反转、MQ-2或LVGL误写成M4已实现
```

- [ ] **Step 5: 由助手提交M4验收结果**

提交信息：

```text
feat: validate TB6612 open-loop motor control
```

- [ ] **Step 6: 由助手合并并推送**

只有最终构建和硬件验收全部通过后，助手才将功能分支合并回`main`并推送GitHub。网络不可用时保留本地提交与领先状态，网络恢复后补推送，不重复M4硬件测试。

- [ ] **Step 7: 转入M5设计阶段**

M5开始前重新核对编码器资料：

```text
橙色VCC
黄色C2
白色C1
红色GND
```

必须确认C1/C2输出类型、上拉要求、每圈计数和方向关系后，再设计TIM3编码器模式。M4结论不能直接替代M5编码器电气验证。
