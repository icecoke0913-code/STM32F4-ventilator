# M3A DHT11 Acquisition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有SmartHood固件中加入每2秒读取一次的DHT11采集任务，实现USART1日志、超时/校验错误报告和断线自动恢复，同时保持M1与M2功能正常。

**Architecture:** CubeMX生成PD0、TIM5和SensorTask基础代码；独立BSP驱动使用TIM5的1 MHz自由运行计数器测量DHT11单总线脉宽；应用任务负责周期调用与日志格式化。DebugLog增加CMSIS-RTOS2互斥锁，使默认任务和传感器任务可以安全共用USART1。

**Tech Stack:** STM32F407VET6、STM32CubeMX 6.16.1、STM32CubeF4 1.28.3、HAL、FreeRTOS CMSIS_V2、Keil MDK-ARM 5/ARMCC 5.06、USART1、TIM5、GPIO。

---

## 文件结构与职责

本计划创建或修改以下文件：

```text
firmware/SmartHood/SmartHood.ioc
    CubeMX配置源：PD0、TIM5、sensorTask。

firmware/SmartHood/Core/Inc/tim.h
firmware/SmartHood/Core/Src/tim.c
    CubeMX生成TIM5句柄和1 MHz自由运行计数器初始化。

firmware/SmartHood/Core/Src/main.c
    CubeMX生成MX_TIM5_Init()调用。

firmware/SmartHood/Core/Src/freertos.c
    CubeMX生成sensorTask；USER CODE中初始化DebugLog并委托App_SensorTask。

firmware/SmartHood/App/Inc/debug_log.h
firmware/SmartHood/App/Src/debug_log.c
    为USART1阻塞日志增加CMSIS-RTOS2互斥保护。

firmware/SmartHood/BSP/Inc/bsp_dht11.h
firmware/SmartHood/BSP/Src/bsp_dht11.c
    DHT11 GPIO时序、TIM5微秒计时、5字节采集和校验。

firmware/SmartHood/App/Inc/app_tasks.h
firmware/SmartHood/App/Src/app_tasks.c
    新增App_SensorTask，每2秒读取并输出一次DHT11状态。

firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
    Keil工程加入bsp_dht11.c；CubeMX自动维护Core生成文件。

docs/project-guide.md
docs/hardware-connections.md
docs/test-records.md
    记录配置、构建、接线、故障和验收结果。
```

本项目没有可直接模拟DHT11微秒波形的主机单元测试框架，也不为HAL编写行为模拟。验证采用逐层检查：CubeMX生成检查、空外设全量编译、接口链接检查、未连接TIMEOUT测试、正常传感器测试和断线恢复测试。

---

### Task 1: 建立M3A开发检查点

**Files:**
- Inspect: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`
- Inspect: `firmware/SmartHood/MDK-ARM/SmartHood.build_log.htm`
- Inspect: `firmware/SmartHood/SmartHood.ioc`

- [ ] **Step 1: 由助手检查Git基线**

应满足：

```text
当前分支：main
工作区：干净
本地至少包含提交c2f1ec3
```

若远端网络恢复，助手先推送尚未同步的设计提交；推送失败不阻止本地M3A教学，但必须明确记录本地领先状态。

- [ ] **Step 2: 由助手创建功能分支**

分支名：

```text
codex/feature-m3a-dht11
```

所有Git命令、暂存、提交、推送和最终合并均由助手执行，用户不需要在PowerShell中操作Git。

- [ ] **Step 3: 用户在Keil执行M2基线全量编译**

操作：

```text
打开 firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
Project → Rebuild all target files
```

预期：

```text
0 Error(s), 0 Warning(s)
Program Size接近M2最终记录：
Code=21234, RO-data=1238, RW-data=148, ZI-data=39452
```

允许因Keil工程元数据产生很小差异，但不得出现未解释的错误或警告。用户把完整构建摘要发给助手后再继续。

---

### Task 2: 在CubeMX配置PD0和TIM5

**Files:**
- Modify: `firmware/SmartHood/SmartHood.ioc`
- Generate: `firmware/SmartHood/Core/Inc/tim.h`
- Generate: `firmware/SmartHood/Core/Src/tim.c`
- Modify: `firmware/SmartHood/Core/Src/main.c`

- [ ] **Step 1: 打开现有IOC并配置PD0**

在CubeMX打开：

```text
firmware/SmartHood/SmartHood.ioc
```

单击PD0，选择：

```text
GPIO_Input
```

在System Core → GPIO中设置：

```text
Pin Name：DHT11_DATA
GPIO mode：Input mode
GPIO Pull-up/Pull-down：Pull-up
```

这里启用内部上拉的目的，是在DHT11拔掉时让PD0保持确定的高电平；正常连接后，三针模块的板载上拉仍是主要上拉。

- [ ] **Step 2: 启用TIM5内部时钟**

在Timers → TIM5中选择：

```text
Clock Source：Internal Clock
```

设置Parameter Settings：

```text
Prescaler (PSC)：83
Counter Mode：Up
Counter Period (ARR)：4294967295
Internal Clock Division：No Division
Auto-reload preload：Disable
```

计算依据：

```text
APB1 Timer Clock = 84 MHz
84 MHz / (83 + 1) = 1 MHz
1个计数 = 1 μs
```

- [ ] **Step 3: 确认TIM5不使用中断**

在TIM5的NVIC Settings中确认：

```text
TIM5 global interrupt：未勾选
```

M3A只读取硬件计数器，不使用更新中断、DMA、输入捕获或PWM。

- [ ] **Step 4: 截图核对后再生成**

向助手提供两张截图：

```text
1. PD0的GPIO配置
2. TIM5 Parameter Settings与NVIC Settings
```

截图核对通过前不要Generate Code。

---

### Task 3: 在CubeMX创建sensorTask并生成代码

**Files:**
- Modify: `firmware/SmartHood/SmartHood.ioc`
- Modify: `firmware/SmartHood/Core/Src/freertos.c`
- Modify: `firmware/SmartHood/Core/Src/main.c`
- Create: `firmware/SmartHood/Core/Inc/tim.h`
- Create: `firmware/SmartHood/Core/Src/tim.c`

- [ ] **Step 1: 新建FreeRTOS任务**

进入Middleware and Software Packs → FREERTOS → Tasks and Queues，保留现有defaultTask并新增：

```text
Task Name：sensorTask
Priority：Normal
Stack Size：256 Words
Entry Function：StartSensorTask
Code Generation Option：Default
Parameter：NULL
Allocation：Dynamic
Buffer Name：NULL
Control Block Name：NULL
```

不创建队列、信号量、软件定时器或额外互斥量；DebugLog互斥量由应用模块创建。

- [ ] **Step 2: 检查FreeRTOS总堆**

保持：

```text
configTOTAL_HEAP_SIZE = 32768 Bytes
```

两个256 Words任务栈和一个互斥量可以容纳在当前32 KB FreeRTOS堆中，不需要扩大。

- [ ] **Step 3: 生成代码**

点击Generate Code。若CubeMX提示打开工程，选择Open Project或回到Keil手动打开工程均可。

- [ ] **Step 4: 生成后保护点检查**

在`firmware/SmartHood/Core/Src/main.c`中应出现：

```c
#include "tim.h"
```

外设初始化区域应包含：

```c
MX_TIM5_Init();
```

在`firmware/SmartHood/Core/Src/freertos.c`中，原有委托必须仍存在：

```c
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
    App_DefaultTask(argument);
  /* USER CODE END StartDefaultTask */
}
```

同时应生成`StartSensorTask()`及sensorTask属性。此时它仍是CubeMX默认空循环，尚未接入应用层。

- [ ] **Step 5: 用户执行空外设全量编译**

Keil执行：

```text
Project → Rebuild all target files
```

预期：

```text
构建日志包含tim.c
0 Error(s), 0 Warning(s)
```

若旧DFP导致目标名称变化，只在Keil明确报设备错误时恢复已验证的STM32F407VE/Flash算法配置；不要预先改动。

- [ ] **Step 6: 由助手提交CubeMX检查点**

建议提交信息：

```text
feat: configure TIM5 and DHT11 sensor task
```

提交前助手检查IOC、生成文件、M1/M2 USER CODE保护点和用户提供的构建结果。

---

### Task 4: 使DebugLog支持多任务并发

**Files:**
- Modify: `firmware/SmartHood/App/Inc/debug_log.h`
- Modify: `firmware/SmartHood/App/Src/debug_log.c`
- Modify: `firmware/SmartHood/Core/Src/freertos.c`

- [ ] **Step 1: 更新debug_log.h接口**

将`firmware/SmartHood/App/Inc/debug_log.h`完整替换为：

```c
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdbool.h>

bool DebugLog_Init(void);
void DebugLog_Printf(const char *format, ...);

#endif
```

- [ ] **Step 2: 先编译验证接口尚未被调用**

在尚未修改`debug_log.c`和`freertos.c`前执行Build。

预期：

```text
0 Error(s), 0 Warning(s)
```

原因：新增函数声明本身不参与链接。这个检查确认头文件语法和`stdbool.h`与ARMCC 5兼容。

- [ ] **Step 3: 更新debug_log.c实现**

将`firmware/SmartHood/App/Src/debug_log.c`完整替换为：

```c
#include "debug_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "usart.h"

static osMutexId_t debug_log_mutex = NULL;

bool DebugLog_Init(void)
{
    if (debug_log_mutex != NULL)
    {
        return true;
    }

    debug_log_mutex = osMutexNew(NULL);
    return debug_log_mutex != NULL;
}

void DebugLog_Printf(const char *format, ...)
{
    char buffer[160];
    va_list arguments;
    int length;

    if ((format == NULL) || (debug_log_mutex == NULL))
    {
        return;
    }

    if (osMutexAcquire(debug_log_mutex, osWaitForever) != osOK)
    {
        return;
    }

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length > 0)
    {
        if (length >= (int)sizeof(buffer))
        {
            length = (int)sizeof(buffer) - 1;
        }

        (void)HAL_UART_Transmit(&huart1,
                                (uint8_t *)buffer,
                                (uint16_t)length,
                                100U);
    }

    (void)osMutexRelease(debug_log_mutex);
}
```

- [ ] **Step 4: 在freertos.c中初始化互斥量**

在`firmware/SmartHood/Core/Src/freertos.c`的USER CODE Includes区域保留`app_tasks.h`并加入：

```c
/* USER CODE BEGIN Includes */

#include "app_tasks.h"
#include "debug_log.h"

/* USER CODE END Includes */
```

在`MX_FREERTOS_Init()`的USER CODE Init区域写入：

```c
  /* USER CODE BEGIN Init */

  if (!DebugLog_Init())
  {
    Error_Handler();
  }

  /* USER CODE END Init */
```

此位置运行在`osKernelInitialize()`之后、任务创建之前，可以创建CMSIS-RTOS2互斥量，并保证两个任务启动前日志模块已就绪。

- [ ] **Step 5: 全量编译验证日志互斥量**

预期：

```text
compiling debug_log.c...
compiling freertos.c...
0 Error(s), 0 Warning(s)
```

- [ ] **Step 6: 烧录进行M1/M2日志回归**

此时sensorTask仍为空循环。烧录后应继续看到：

```text
SmartHood M1 start
ST7735S init and test OK
heartbeat=... key=... tick=...
```

PA0、PA1和TFT现象保持不变。

- [ ] **Step 7: 由助手提交日志保护检查点**

建议提交信息：

```text
refactor: make debug logging thread safe
```

---

### Task 5: 创建DHT11公共接口

**Files:**
- Create: `firmware/SmartHood/BSP/Inc/bsp_dht11.h`

- [ ] **Step 1: 创建bsp_dht11.h**

创建`firmware/SmartHood/BSP/Inc/bsp_dht11.h`，完整内容为：

```c
#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DHT11_STATUS_OK = 0,
    DHT11_STATUS_TIMEOUT,
    DHT11_STATUS_CHECKSUM_ERROR
} DHT11_Status_t;

typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
} DHT11_Data_t;

bool BSP_DHT11_Init(void);
DHT11_Status_t BSP_DHT11_Read(DHT11_Data_t *data);

#endif
```

接口约定：`BSP_DHT11_Read()`的调用者必须传入有效的非NULL指针；只有返回`DHT11_STATUS_OK`时，结构体中的数据才会更新。

- [ ] **Step 2: 在Keil中确认BSP包含路径**

Options for Target → C/C++ → Include Paths中应已存在：

```text
..\BSP\Inc
```

这是M2已建立的路径，不重复添加。

- [ ] **Step 3: 头文件语法检查**

暂时在`app_tasks.c`包含区加入：

```c
#include "bsp_dht11.h"
```

执行Build。预期：

```text
0 Error(s), 0 Warning(s)
```

此时不要调用DHT11函数，否则驱动源文件尚未建立会产生未定义符号。

---

### Task 6: 实现DHT11阻塞式BSP驱动

**Files:**
- Create: `firmware/SmartHood/BSP/Src/bsp_dht11.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [ ] **Step 1: 创建bsp_dht11.c**

创建`firmware/SmartHood/BSP/Src/bsp_dht11.c`，完整内容为：

```c
#include "bsp_dht11.h"

#include <stddef.h>

#include "main.h"
#include "tim.h"

#define DHT11_START_LOW_US       18000U
#define DHT11_EDGE_TIMEOUT_US      120U
#define DHT11_ONE_THRESHOLD_US      50U
#define DHT11_DATA_BYTES              5U
#define DHT11_DATA_BITS              40U

static void DHT11_SetOutputLow(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_RESET);

    gpio_init.Pin = DHT11_DATA_Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &gpio_init);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT11_DATA_Pin;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &gpio_init);
}

static uint32_t DHT11_GetTimeUs(void)
{
    return __HAL_TIM_GET_COUNTER(&htim5);
}

static void DHT11_DelayUs(uint32_t delay_us)
{
    uint32_t start_time = DHT11_GetTimeUs();

    while ((uint32_t)(DHT11_GetTimeUs() - start_time) < delay_us)
    {
    }
}

static bool DHT11_WaitForPin(GPIO_PinState expected_state,
                             uint32_t timeout_us)
{
    uint32_t start_time = DHT11_GetTimeUs();

    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port,
                           DHT11_DATA_Pin) != expected_state)
    {
        if ((uint32_t)(DHT11_GetTimeUs() - start_time) >= timeout_us)
        {
            return false;
        }
    }

    return true;
}

bool BSP_DHT11_Init(void)
{
    DHT11_SetInput();
    __HAL_TIM_SET_COUNTER(&htim5, 0U);

    return HAL_TIM_Base_Start(&htim5) == HAL_OK;
}

DHT11_Status_t BSP_DHT11_Read(DHT11_Data_t *data)
{
    uint8_t raw_data[DHT11_DATA_BYTES] = {0U};
    uint32_t saved_primask;
    uint32_t high_start;
    uint32_t high_width;
    uint32_t bit_index;
    uint32_t byte_index;
    bool capture_ok = true;
    uint16_t humidity_x10;
    int16_t temperature_x10;

    DHT11_SetOutputLow();
    DHT11_DelayUs(DHT11_START_LOW_US);
    DHT11_SetInput();

    saved_primask = __get_PRIMASK();
    __disable_irq();

    if (!DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US) ||
        !DHT11_WaitForPin(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US) ||
        !DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US))
    {
        capture_ok = false;
    }

    for (bit_index = 0U;
         capture_ok && (bit_index < DHT11_DATA_BITS);
         bit_index++)
    {
        if (!DHT11_WaitForPin(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US))
        {
            capture_ok = false;
            break;
        }

        high_start = DHT11_GetTimeUs();

        if (!DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US))
        {
            capture_ok = false;
            break;
        }

        high_width = (uint32_t)(DHT11_GetTimeUs() - high_start);
        byte_index = bit_index / 8U;
        raw_data[byte_index] <<= 1U;

        if (high_width > DHT11_ONE_THRESHOLD_US)
        {
            raw_data[byte_index] |= 1U;
        }
    }

    if (saved_primask == 0U)
    {
        __enable_irq();
    }

    DHT11_SetInput();

    if (!capture_ok)
    {
        return DHT11_STATUS_TIMEOUT;
    }

    if ((uint8_t)(raw_data[0] + raw_data[1] +
                  raw_data[2] + raw_data[3]) != raw_data[4])
    {
        return DHT11_STATUS_CHECKSUM_ERROR;
    }

    humidity_x10 = (uint16_t)raw_data[0] * 10U + raw_data[1];
    temperature_x10 = (int16_t)
        (((uint16_t)(raw_data[2] & 0x7FU) * 10U) + raw_data[3]);

    if ((raw_data[2] & 0x80U) != 0U)
    {
        temperature_x10 = (int16_t)-temperature_x10;
    }

    data->humidity_x10 = humidity_x10;
    data->temperature_x10 = temperature_x10;

    return DHT11_STATUS_OK;
}
```

- [ ] **Step 2: 检查关键安全出口**

逐项人工核对：

```text
所有等待调用DHT11_WaitForPin()并带120 μs超时
18 ms启动阶段没有关闭中断
关中断后不存在直接return
saved_primask为0时才重新开中断
TIMEOUT和CHECKSUM_ERROR不会写入data
PD0在退出前恢复输入上拉
```

- [ ] **Step 3: 将源文件加入Keil BSP分组**

在Keil中：

```text
右键BSP组 → Add Existing Files to Group 'BSP'...
选择 ..\BSP\Src\bsp_dht11.c
文件类型选择C Source file
```

BSP组最终应包含：

```text
fonts.c
bsp_st7735s.c
bsp_dht11.c
```

- [ ] **Step 4: 编译尚未调用的驱动**

执行Rebuild。预期日志包含：

```text
compiling bsp_dht11.c...
0 Error(s), 0 Warning(s)
```

由于应用层尚未调用驱动，链接器可能移除部分DHT11代码，因此程序大小变化不是本检查点的判断标准。

- [ ] **Step 5: 由助手提交BSP检查点**

建议提交信息：

```text
feat: add TIM5-based DHT11 BSP driver
```

---

### Task 7: 接入SensorTask和DHT11日志

**Files:**
- Modify: `firmware/SmartHood/App/Inc/app_tasks.h`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/Core/Src/freertos.c`

- [ ] **Step 1: 更新app_tasks.h**

将`firmware/SmartHood/App/Inc/app_tasks.h`完整替换为：

```c
#ifndef APP_TASKS_H
#define APP_TASKS_H

void App_DefaultTask(void *argument);
void App_SensorTask(void *argument);

#endif
```

- [ ] **Step 2: 在app_tasks.c中加入DHT11头文件**

确认文件头部包含：

```c
#include "app_tasks.h"

#include <stdint.h>

#include "bsp_dht11.h"
#include "bsp_st7735s.h"
#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"
```

不要删除现有`App_RunDisplayTest()`和`App_DefaultTask()`。

- [ ] **Step 3: 在app_tasks.c末尾新增App_SensorTask**

在现有`App_DefaultTask()`之后加入：

```c
void App_SensorTask(void *argument)
{
    DHT11_Data_t data = {0};

    (void)argument;

    if (!BSP_DHT11_Init())
    {
        DebugLog_Printf("DHT11 timer start failed\r\n");

        for (;;)
        {
            osDelay(2000U);
        }
    }

    osDelay(2000U);

    for (;;)
    {
        DHT11_Status_t status = BSP_DHT11_Read(&data);

        if (status == DHT11_STATUS_OK)
        {
            int32_t temperature_x10 = data.temperature_x10;
            uint32_t temperature_magnitude;

            if (temperature_x10 < 0)
            {
                temperature_magnitude = (uint32_t)(-temperature_x10);
            }
            else
            {
                temperature_magnitude = (uint32_t)temperature_x10;
            }

            DebugLog_Printf(
                "DHT11 temp=%s%lu.%luC humidity=%lu.%lu%% status=OK\r\n",
                (temperature_x10 < 0) ? "-" : "",
                (unsigned long)(temperature_magnitude / 10U),
                (unsigned long)(temperature_magnitude % 10U),
                (unsigned long)(data.humidity_x10 / 10U),
                (unsigned long)(data.humidity_x10 % 10U));
        }
        else if (status == DHT11_STATUS_CHECKSUM_ERROR)
        {
            DebugLog_Printf("DHT11 status=CHECKSUM_ERROR\r\n");
        }
        else
        {
            DebugLog_Printf("DHT11 status=TIMEOUT\r\n");
        }

        osDelay(2000U);
    }
}
```

首次读取前延迟2秒，用于等待DHT11上电稳定。之后每次读取完成后延迟2秒，实际日志周期会比2秒多约18 ms，符合本阶段要求。

- [ ] **Step 4: 委托CubeMX生成的StartSensorTask**

在`firmware/SmartHood/Core/Src/freertos.c`找到CubeMX生成的`StartSensorTask()`，将USER CODE区域改为：

```c
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  App_SensorTask(argument);
  /* USER CODE END StartSensorTask */
}
```

只修改USER CODE区域，不删除CubeMX生成的函数声明、任务句柄或属性。

- [ ] **Step 5: 全量编译验证完整链接**

执行Rebuild。预期：

```text
compiling app_tasks.c...
compiling freertos.c...
compiling bsp_dht11.c...
linking...
0 Error(s), 0 Warning(s)
```

程序Code和RO-data应比M2增加，证明DHT11驱动和日志逻辑进入最终镜像；不预设精确字节数。

- [ ] **Step 6: 由助手提交任务集成检查点**

建议提交信息：

```text
feat: integrate periodic DHT11 sensor task
```

---

### Task 8: 未连接DHT11的安全测试

**Files:**
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood.hex`
- Update after test: `docs/test-records.md`

- [ ] **Step 1: 保持DHT11完全未连接**

此测试只连接当前已验证的开发板、ST-Link、USB转TTL和TFT。MQ-2、TB6612、电机和DHT11均不连接。

- [ ] **Step 2: 烧录并复位**

预期启动日志：

```text
SmartHood M1 start
ST7735S init and test OK
```

约2秒后开始出现：

```text
DHT11 status=TIMEOUT
```

后续心跳和DHT11日志应交替出现，示例顺序不要求完全固定：

```text
heartbeat=2 key=0 tick=...
DHT11 status=TIMEOUT
heartbeat=3 key=0 tick=...
```

- [ ] **Step 3: 验证TIMEOUT不会阻塞系统**

连续观察至少30秒：

```text
心跳序号持续递增
PA1继续每秒翻转
PA0按下时日志key=1，松开后恢复key=0
TFT保持M2最终测试画面
无重复SmartHood M1 start
无日志乱码或半行交叉
```

若DHT11未连接却频繁报告CHECKSUM_ERROR而不是TIMEOUT，先检查PD0是否确实配置了内部Pull-up。

- [ ] **Step 4: 记录未连接测试结果**

在`docs/test-records.md`的M3-T1记录中追加构建大小、TIMEOUT日志样本、30秒观察结果和M1/M2回归结果。

- [ ] **Step 5: 由助手提交安全测试记录**

建议提交信息：

```text
test: validate DHT11 timeout behavior
```

---

### Task 9: 连接DHT11并验证正常采集

**Files:**
- Verify: `docs/hardware-connections.md`
- Update: `docs/test-records.md`

- [ ] **Step 1: 断开全部供电**

在移动接线前断开：

```text
开发板USB-C
ST-Link USB
USB转TTL
```

确认TFT背光和开发板电源灯均熄灭后再接DHT11。

- [ ] **Step 2: 确认DHT11三针丝印**

实物必须能明确识别：

```text
VCC或+
DATA、OUT或S
GND或-
```

若丝印不清楚，用户发送正反面清晰照片，由助手核对后继续。不得根据常见模块针序直接猜测。

- [ ] **Step 3: 按确认后的信号连接**

```text
DHT11 VCC  → STM32 3.3V
DHT11 DATA → STM32 PD0
DHT11 GND  → STM32 GND
```

MQ-2保持不连接。不要将DHT11接到5V。

- [ ] **Step 4: 上电观察自动恢复**

无需重新编译。上电后预期在首次2秒等待结束后出现：

```text
DHT11 temp=xx.xC humidity=xx.x% status=OK
```

若最初一两次TIMEOUT后恢复OK，可以记录但不立即判定失败；先检查接触稳定性并观察后续读数。

- [ ] **Step 5: 连续读取验收**

记录至少10次连续OK读数，并核对：

```text
温度处于合理室内范围
湿度处于0.0%～100.0%
没有乱码或日志交叉
心跳继续递增
PA0、PA1和TFT保持正常
```

- [ ] **Step 6: 湿度变化测试**

距DHT11适当距离缓慢哈气一次，不要让水汽凝结。接下来数个采样周期内湿度应出现可观察变化，并逐渐回落。

---

### Task 10: 验证运行中断线与自动恢复

**Files:**
- Update: `docs/test-records.md`
- Update: `docs/project-guide.md`
- Update: `docs/hardware-connections.md`

- [ ] **Step 1: 记录断线前有效值**

确认串口正在稳定输出OK日志，记录最后一条温湿度值。

- [ ] **Step 2: 只断开DATA线**

为降低误接风险，优先断开DHT11模块一侧的DATA杜邦线，不移动3.3V和GND。预期后续周期出现：

```text
DHT11 status=TIMEOUT
```

同时确认默认任务心跳、PA0、PA1和TFT没有中断。

- [ ] **Step 3: 重新接回DATA线**

不按RST、不重新烧录。预期在后续读取周期自动恢复：

```text
DHT11 temp=xx.xC humidity=xx.x% status=OK
```

- [ ] **Step 4: 重复一次断线恢复**

再次执行一次DATA断开和接回，确认行为可重复。若恢复后持续CHECKSUM_ERROR，断电后重新插紧DATA线再测试，不能通过反复修改时序阈值掩盖接触问题。

- [ ] **Step 5: 完成M3-T1记录**

文档至少记录：

```text
CubeMX：PD0 Pull-up、TIM5 PSC=83、ARR=0xFFFFFFFF、无中断
SensorTask：Normal、256 Words、2秒周期
最终构建大小和0错误/0警告结果
10次正常读数结果
湿度变化现象
断线TIMEOUT日志
两次自动恢复结果
PA0、PA1、USART1和TFT回归结果
发现的问题及解决方式
```

- [ ] **Step 6: 由助手提交M3A验收结果**

建议提交信息：

```text
feat: validate DHT11 acquisition and recovery
```

---

### Task 11: M3A收尾与主分支集成

**Files:**
- Verify: all modified project files
- Update if needed: `docs/project-guide.md`

- [ ] **Step 1: 最终全量编译**

用户在Keil执行Rebuild，并提供完整摘要。必须满足：

```text
0 Error(s), 0 Warning(s)
```

- [ ] **Step 2: 最终硬件回归**

至少观察1分钟：

```text
DHT11持续输出OK
心跳持续输出
PA0按下/松开正确
PA1每秒翻转
TFT显示稳定
无异常复位、明显发热或日志交叉
```

- [ ] **Step 3: 由助手检查Git与文档一致性**

检查内容：

```text
IOC与生成代码一致
Keil工程包含bsp_dht11.c
没有遗留未跟踪源码
设计、实施计划、接线和测试记录一致
MQ-2仍明确标记为未接入
```

- [ ] **Step 4: 由助手合并回main并推送**

只有最终构建和硬件验收均通过后，助手才将功能分支合并回`main`并推送GitHub。网络不可用时保留本地提交和领先状态，网络恢复后补推送，不重复实施M3A。

- [ ] **Step 5: 进入M3B或下一可执行里程碑**

M3B MQ-2仍需要：

```text
万用表
10 kΩ电阻
15 kΩ电阻
面包板或可靠焊接转接
```

如果这些条件尚未具备，M3A完成后暂停MQ-2，转而讨论是否进入M4 TB6612与电机开环PWM；不能把MQ-2 AO或DO直接接STM32。
