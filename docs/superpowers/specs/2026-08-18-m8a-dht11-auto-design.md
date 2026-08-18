# M8A DHT11自动模式设计

## 目标

在不接入MQ-2的前提下，使用DHT11温度和湿度驱动AUTO模式，验证自动目标请求、迟滞、传感器失效安全停止以及与现有MANUAL/故障控制的兼容性。

M8A不声称真实烟雾浓度，不实现MQ-2融合，不实现BACKFLOW的真实气流判断。MQ-2保留到后续具备万用表、分压电阻和可靠接线条件后再接入。

## 已确认条件

- MCU：STM32F407VET6，FreeRTOS/CMSIS-RTOS v2，Keil MDK-ARM V5。
- DHT11：PD0，当前已验证约2秒读取周期、校验错误和断线自动恢复。
- 电机：MotorTask是唯一PWM所有者，复用M6的30%软启动和相对计数PI闭环。
- M7模式：上电为`STANDBY + AUTO + LOW`；长按切换运行许可；短按循环`AUTO -> MANUAL -> BACKFLOW`；双击只在MANUAL切换LOW/HIGH。
- MQ-2：商家手册明确AO输出为`0~5V`，当前AO/DO均保持断开。

## AUTO策略

### 运行许可

AUTO只有在`run_state=RUNNING`且没有编码器故障时才允许产生运行请求。上电、RST、长按进入STANDBY和故障锁存均优先停止电机。

### 传感器有效性

SensorTask每约2秒读取一次DHT11。只有`DHT11_STATUS_OK`才发布新快照；TIMEOUT或CHECKSUM_ERROR只记录日志，不覆盖最后一帧有效数据。

快照超过6000ms没有更新时视为过期，AUTO返回STOP。6000ms覆盖三个DHT11采样周期，能够容忍一次读取失败，同时避免长期使用旧环境数据。

### 三状态阈值

| 当前AUTO目标 | 保持或进入条件 | 结果 |
|---|---|---|
| STOP | 温度`<28.0C`且湿度`<70%` | 电机停止 |
| LOW | 温度`>=28.0C`或湿度`>=70%` | 低档闭环 |
| HIGH | 温度`>=32.0C`或湿度`>=85%` | 高档闭环 |

迟滞规则：

- HIGH只有在温度`<=31.0C`且湿度`<=80%`时降为LOW；
- LOW只有在温度`<=27.0C`且湿度`<=65%`时降为STOP；
- 只要任一输入达到更高档的进入阈值，就进入更高档；
- 温度和湿度均为DHT11的定点整数值，不使用浮点运算。

## 软件架构

### SensorSnapshot

在Control层定义由算法和采集任务共同使用的共享快照：

```c
typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
    bool valid;
    uint32_t updated_tick;
} AutoPolicySnapshot_t;
```

快照由SensorTask写入、MotorTask读取。访问通过一个CMSIS-RTOS互斥量保护，读写都复制完整结构后立即释放互斥量；DHT11读取本身不在互斥量内执行。

`App_MotorControl_Init()`在创建按键队列时一并创建传感器互斥量。创建失败调用`Error_Handler()`，不启动任务。

### AutoPolicy

新增`Control/Inc/auto_policy.h`、`Control/Src/auto_policy.c`和`Control/Test/auto_policy_selftest.c`。

纯函数接口：

```c
ModeMotorRequest_t AutoPolicy_Evaluate(
    const AutoPolicySnapshot_t *snapshot,
    uint32_t now_tick,
    ModeMotorRequest_t previous_auto_request);
```

该函数只负责有效性、过期判断和阈值迟滞，不读取硬件、不修改PWM、不访问FreeRTOS对象。

### ModeManager接口

将电机请求映射扩展为：

```c
ModeMotorRequest_t ModeManager_GetMotorRequest(
    const ModeManager_t *manager,
    ModeMotorRequest_t auto_request);
```

只有在`mode=MODE_AUTO`、运行许可有效且无故障时才采用`auto_request`；MANUAL继续根据LOW/HIGH预选生成请求；BACKFLOW仍返回STOP。空指针、非法枚举、故障和STANDBY全部安全停止或返回FAULT。

### MotorTask数据流

每50ms执行：

1. 读取并复制传感器快照；
2. 调用`AutoPolicy_Evaluate()`得到AUTO候选请求；
3. 调用`ModeManager_GetMotorRequest()`得到最终请求；
4. 仅当最终请求变化时重新进入对应的STOP、LOW_START或HIGH_START；
5. 继续执行现有编码器采样、PI、无反馈故障和PWM输出。

AUTO从STOP进入LOW/HIGH，以及LOW/HIGH互换时，复用现有30%/300ms软启动。AUTO进入STOP时立即PWM归零并清理PI积分。

## 错误和安全策略

- DHT11读取失败：不更新快照；快照超过6000ms后AUTO停止。
- 传感器互斥量创建或获取失败：本周期AUTO停止，不影响MANUAL故障停机。
- 编码器无反馈故障优先于所有AUTO请求，仍由长按清除并恢复安全初始状态。
- AUTO策略永远不直接调用电机驱动函数。
- MQ-2 AO、DO在M8A中不加入CubeMX接线、不加入ADC读取、不加入控制计算。

## 自检和验收

### 纯算法自检

覆盖有效快照、过期快照、STOP/LOW/HIGH进入、HIGH降档、LOW停止、边界值和非法请求；每个测试使用独立状态，明确断言中间状态不提前改变。

### 板端验收顺序

1. 保持TB6612 VM断开，烧录临时自检版本，确认串口输出`M8A auto policy self-test PASSED`。
2. 连接DHT11，观察有效温湿度日志和快照年龄；断开DHT11后确认6秒后AUTO停止，重新连接后自动恢复有效状态。
3. VM仍断开时验证AUTO/MANUAL/BACKFLOW切换、STANDBY和RST停止路径。
4. 关闭临时自检并完整Rebuild，确认`0 Error(s), 0 Warning(s)`。
5. 禁止安装扇叶和机械负载，完全断电后接通VM，仅验证AUTO低档/高档/停止趋势和无异常声音、异味、明显发热或复位。

## 非目标

- 不把DHT11温湿度换算为烟雾浓度或ppm。
- 不在M8A接入MQ-2 AO/DO。
- 不实现BACKFLOW真实检测或反转电机。
- 不修改M7按键时序、编码器故障清除规则和M6 PI参数。
- 不提前移植LVGL；正式UI仍留到M9。
