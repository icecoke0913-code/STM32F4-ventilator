# M5 Encoder Measurement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 接入JGA12-N20-50B编码器，使用TIM3硬件正交计数完成方向、计数增量和RPM测量，并通过实际输出轴转数完成标定。

**Architecture:** PC6/PC7分别作为TIM3_CH1/CH2，TIM3使用Encoder Mode硬件计数；独立`bsp_encoder`封装计数器访问与整数RPM换算；新增`motorTask`每50 ms采样、每500 ms输出日志。M5不修改M4的PA0开环PWM控制，不实现PID。

**Tech Stack:** STM32F407VET6、STM32CubeMX 6.16.1、STM32CubeF4 1.28.3、HAL、FreeRTOS CMSIS-RTOS2、Keil MDK-ARM V5/ARMCC 5.06、TIM3 Encoder Mode、TB6612FNG、JGA12-N20-50B。

---

### Task 1: 建立M5分支与基线

**Files:**
- Verify: `firmware/SmartHood/SmartHood.ioc`
- Verify: `docs/project-guide.md`
- Verify: `docs/test-records.md`

- [x] **Step 1: 创建M5功能分支**

```powershell
git switch -c codex/feature-m5-encoder
```

Expected: 当前分支为`codex/feature-m5-encoder`，工作区干净。

- [x] **Step 2: 保存M4基线构建信息**

确认Keil Rebuild仍为`0 Error(s), 0 Warning(s)`，并记录当前HEX SHA-256：

```powershell
Get-FileHash firmware\SmartHood\MDK-ARM\SmartHood\SmartHood.hex -Algorithm SHA256
```

- [x] **Step 3: 提交M5启动检查点**

```powershell
git add docs/superpowers/specs/2026-08-09-m5-encoder-measurement-design.md
git commit -m "docs: start M5 encoder implementation"
```

### Task 2: 在CubeMX配置TIM3编码器接口

**Files:**
- Modify: `firmware/SmartHood/SmartHood.ioc`
- Generated: `firmware/SmartHood/Core/Inc/tim.h`
- Generated: `firmware/SmartHood/Core/Src/tim.c`
- Generated: `firmware/SmartHood/Core/Src/gpio.c`

- [x] **Step 1: 配置PC6和PC7复用功能**

在Pinout中设置：

```text
PC6 = TIM3_CH1
PC7 = TIM3_CH2
```

确认两脚均为`GPIO_AF2_TIM3`，不要将PC6/PC7配置成普通GPIO或外部中断。

- [x] **Step 2: 启用TIM3 Encoder Mode**

在Timers → TIM3设置：

```text
Mode: Encoder Mode (TI1 and TI2)
Prescaler: 0
Counter Period: 65535
Counter Mode: Up
Clock Division: DIV1
Auto-reload preload: Disable
```

两路Input Capture均设置：

```text
Polarity: Rising Edge
Selection: Direct TI
Prescaler: DIV1
Filter: 4
```

Filter先使用4抑制线缆噪声；若手动慢转时漏计数，再依据测试结果调整。

- [x] **Step 3: 配置输入上下拉**

PC6和PC7选择`Pull-up`、`Low Speed`，不启用TIM3中断。

- [x] **Step 4: 生成并核对代码**

生成后确认`MX_TIM3_Init()`包含`HAL_TIM_Encoder_Init()`和两路`HAL_TIM_Encoder_ConfigChannel()`，并保留所有USER CODE区域。

- [x] **Step 5: 提交CubeMX检查点**

```powershell
git add firmware/SmartHood/SmartHood.ioc firmware/SmartHood/Core/Inc/tim.h firmware/SmartHood/Core/Src/tim.c firmware/SmartHood/Core/Src/gpio.c
git commit -m "feat: configure TIM3 encoder interface"
```

### Task 3: 空外设构建验证

**Files:**
- Verify: `firmware/SmartHood/Core/Src/tim.c`
- Verify: `firmware/SmartHood/Core/Src/stm32f4xx_hal_msp.c`
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [ ] **Step 1: 核对生成结果**

应看到：

```c
TIM_HandleTypeDef htim3;
htim3.Instance = TIM3;
GPIO_AF2_TIM3
```

- [ ] **Step 2: 确认Keil工程没有重复源文件**

确认`tim.c`只出现一次，新的TIM3句柄可以被后续BSP引用。

- [ ] **Step 3: Rebuild**

Expected: `0 Error(s), 0 Warning(s)`。出现句柄未定义或重复文件时，先修复工程再继续。

### Task 4: 编写带中文注释的编码器BSP

**Files:**
- Create: `firmware/SmartHood/BSP/Inc/bsp_encoder.h`
- Create: `firmware/SmartHood/BSP/Src/bsp_encoder.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [ ] **Step 1: 定义公共接口**

```c
void BSP_Encoder_Init(void);
void BSP_Encoder_Start(void);
void BSP_Encoder_Stop(void);
int16_t BSP_Encoder_ReadDelta(void);
int32_t BSP_Encoder_CountToRpmX10(int16_t delta, uint32_t sample_ms);
```

每个函数和宏添加中文注释，写明单位、范围和回绕约束。

- [ ] **Step 2: 实现回绕安全差值**

```c
uint16_t current = __HAL_TIM_GET_COUNTER(&htim3);
uint16_t delta_u16 = (uint16_t)(current - s_last_count);
s_last_count = current;
return (int16_t)delta_u16;
```

50 ms窗口内增量必须小于32768 counts；300 RPM、1400 counts/圈时约为350 counts，满足约束。

- [ ] **Step 3: 实现整数RPM换算**

```c
rpm_x10 = abs(delta) * 600000UL / (counts_per_rev * sample_ms);
```

`counts_per_rev`初始为1400，使用单一宏保存，后续用实测标定值替换。

- [ ] **Step 4: 加入Keil BSP分组并编译**

将BSP头文件目录加入Include Paths，将`bsp_encoder.c`加入BSP源文件组。Rebuild保持0错误、0警告。

- [ ] **Step 5: 提交BSP检查点**

```powershell
git add firmware/SmartHood/BSP/Inc/bsp_encoder.h firmware/SmartHood/BSP/Src/bsp_encoder.c firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
git commit -m "feat: add TIM3 encoder BSP"
```

### Task 5: 创建motorTask并接入FreeRTOS

**Files:**
- Modify: `firmware/SmartHood/App/Inc/app_tasks.h`
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`
- Modify: `firmware/SmartHood/Core/Src/freertos.c`
- Modify: `firmware/SmartHood/MDK-ARM/SmartHood.uvprojx`

- [ ] **Step 1: 增加任务接口和任务参数**

在`app_tasks.h`声明：

```c
void App_MotorTask(void *argument);
```

在FreeRTOS配置中新增`motorTask`：Normal优先级、256 Words动态栈、入口`StartMotorTask`。不修改现有defaultTask和sensorTask。

- [ ] **Step 2: 保持CubeMX委托结构**

```c
static void StartMotorTask(void *argument)
{
  App_MotorTask(argument);
}
```

- [ ] **Step 3: 实现50 ms采样和500 ms日志**

任务启动TIM3，保存初始计数；之后每50 ms读取增量、判断方向并换算`rpm_x10`，每10次通过`DebugLog_Printf()`输出一次。使用CMSIS-RTOS2的绝对延时或等效固定周期方式，避免任务漂移。

- [ ] **Step 4: 保持M4行为不变**

确认`App_DefaultTask`仍负责PA0挡位和`BSP_Motor_SetDuty()`；`App_MotorTask`只读编码器，不写TIM4 CCR、不改变STBY和方向脚。

- [ ] **Step 5: 提交任务集成检查点**

```powershell
git add firmware/SmartHood/App/Inc/app_tasks.h firmware/SmartHood/App/Src/app_tasks.c firmware/SmartHood/Core/Src/freertos.c firmware/SmartHood/MDK-ARM/SmartHood.uvprojx
git commit -m "feat: add motor encoder measurement task"
```

### Task 6: 无硬件全量构建与静态检查

**Files:**
- Verify: all modified M5 source and generated files

- [ ] **Step 1: 检查中文注释与接口一致性**

确认新增自编代码均有模块职责、公开接口、单位、错误路径和关键参数中文注释；头文件声明与实现签名必须一致。

- [ ] **Step 2: Rebuild Keil工程**

Expected: `0 Error(s), 0 Warning(s)`；记录Code、RO-data、RW-data、ZI-data和Build Time。

- [ ] **Step 3: 检查差异**

```powershell
git diff --check
git status --short
```

Expected: 无空白错误，除计划中的文件外无未跟踪M5源码。

### Task 7: M5硬件验收与标定

**Files:**
- Update: `docs/test-records.md`
- Update: `docs/hardware-connections.md`

- [ ] **Step 1: M5-T1手动计数**

断开TB6612 VM，仅给STM32和编码器3.3V供电；手动转动输出轴，确认计数变化、正反方向符号相反、停止后计数保持。

- [ ] **Step 2: M5-T2信号稳定性**

在无VM状态下测试慢速、快速、正反转切换，记录丢计数、方向跳变或无响应现象。异常时不接通VM，先检查供电、共地、C1/C2和TIM3复用。

- [ ] **Step 3: M5-T3低占空比测速**

接通TB6612 5V VM，按30%→50%→70%短时测试；记录方向、计数增量、稳定RPM、启动和噪声现象。禁止堵转、扇叶和机械负载。

- [ ] **Step 4: M5-T4 RPM标定**

稳定运行约30秒，记录总计数和输出轴实际转数：

```text
实测每圈计数 = 总计数 / 实际输出轴转数
```

后续RPM换算统一使用实测每圈计数，同时保留理论1400 counts/圈。

- [ ] **Step 5: 更新验收文档并提交**

在`docs/test-records.md`记录每项结果、实际计数常数和未测试限制；在`docs/hardware-connections.md`更新PC6/PC7状态。C1/C2输出类型若未实测，必须写“未确认”。

```powershell
git add docs/test-records.md docs/hardware-connections.md
git commit -m "test: record M5 encoder measurement results"
```

### Task 8: M5最终检查、合并与推送

**Files:**
- Verify: `docs/superpowers/specs/2026-08-09-m5-encoder-measurement-design.md`
- Verify: `docs/superpowers/plans/2026-08-09-m5-encoder-measurement.md`
- Verify: all M5 firmware files

- [ ] **Step 1: 确认范围边界**

确认文档和代码没有把PID、自动调速、软件反转、MQ-2或LVGL描述为M5已实现。

- [ ] **Step 2: 最终Rebuild与HEX校验**

记录最终Keil构建摘要和HEX SHA-256，并与测试记录保持一致。

- [ ] **Step 3: 提交最终收尾**

```powershell
git add docs firmware
git commit -m "feat: complete M5 encoder measurement"
```

- [ ] **Step 4: 合并回main并尝试推送**

最终构建和硬件验收全部通过后，将`codex/feature-m5-encoder`合并回`main`并尝试推送GitHub；网络失败时保留本地提交并明确记录领先状态。
