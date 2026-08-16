# M7 单键交互与模式管理设计

## 1. 目标与范围

M7 在 M6 相对速度 PI 闭环基础上，将 PA0 的机械按键识别、系统模式管理和电机控制职责分离，实现可靠的短按、双击和长按交互。

本阶段实现：

- PA0 轮询、消抖、短按、双击和长按识别。
- 待机与运行许可切换。
- `AUTO`、`MANUAL`、`BACKFLOW` 三种模式切换。
- `MANUAL` 模式下低档和高档切换。
- 编码器无反馈故障下的按键限制和人工清除。
- 按键状态机、模式转换表及板端交互验收。

本阶段不实现：

- `AUTO` 模式的温湿度和 MQ-2 融合算法。
- `BACKFLOW` 模式的 MQ-2 阈值和迟滞控制。
- 精确 190 RPM/220 RPM 控制或编码器 CPR 重新标定。
- 软件反转、堵转、过流、过温、带扇叶或带机械负载测试。
- LVGL 和正式 UI。

因此，M7 中 `AUTO` 和 `BACKFLOW` 只维护状态并输出日志，电机始终保持停止。只有运行许可有效且模式为 `MANUAL` 时，才允许使用 M6 已验证的相对速度闭环。

## 2. 已确认的交互参数

| 参数 | 数值 | 说明 |
| --- | ---: | --- |
| 按键扫描周期 | 20 ms | 沿用现有 DefaultTask 快速循环 |
| 消抖时间 | 40 ms | 原始电平连续稳定后才改变稳定状态 |
| 双击窗口 | 350 ms | 第一次点击后等待第二次点击 |
| 长按阈值 | 1000 ms | 达到阈值立即上报一次长按 |
| PA0 有效电平 | 高电平 | 板载按键按下为高，GPIO 内部下拉 |

单击必须等待双击窗口结束后才确认。长按达到阈值时只上报一次，释放时不再产生短按。所有时间差使用 32 位无符号减法，以兼容毫秒 Tick 回绕。

## 3. 模式含义

### 3.1 AUTO

自动模式将在 M8 中根据 DHT11 温湿度和 MQ-2 烟雾相对值生成目标速度。M7 只保留模式状态，目标输出固定为停止。

### 3.2 MANUAL

手动模式使用 M6 的两个相对计数目标：

- 低档：130 counts/50 ms，前馈 50%。
- 高档：195 counts/50 ms，前馈 70%。

由于实际 CPR 标定由用户跳过，本阶段继续使用 `LOW` 和 `HIGH` 描述挡位，不把它们标记为精确 190 RPM 和 220 RPM。

### 3.3 BACKFLOW

防回流模式将在 M8 中使用 MQ-2 开启阈值和关闭阈值进行迟滞控制：超过开启阈值时运行高档，下降到关闭阈值以下才停止。M7 只保留模式状态，目标输出固定为停止。

`BACKFLOW` 不表示电机反转。TB6612 仍保持当前固定方向控制，不新增软件反转接口。

## 4. 模块边界与数据流

```text
PA0 GPIO
   │ 原始电平和当前Tick，每20ms调用
   ▼
BSP_Key_Process()
   │ NONE / SHORT / DOUBLE / LONG
   ▼
CMSIS-RTOS2按键事件队列
   │ MotorTask按产生顺序取出事件
   ▼
ModeManager_HandleEvent()
   │ 系统状态、模式、手动挡位、故障状态
   ▼
MotorTask中的M6软启动、PI和故障状态机
   │ 唯一允许调用BSP_Motor
   ▼
TB6612 + 电机
```

新增文件：

- `BSP/Inc/bsp_key.h`：按键上下文、时间参数、事件类型和公开接口。
- `BSP/Src/bsp_key.c`：消抖及短按、双击、长按状态机。
- `BSP/Test/bsp_key_selftest.h/.c`：固定时间序列按键自检。
- `Control/Inc/mode_manager.h`：系统状态、模式、挡位、故障和状态快照接口。
- `Control/Src/mode_manager.c`：纯模式转换状态机。
- `Control/Test/mode_manager_selftest.h/.c`：模式转换表自检。

`App_DefaultTask()`只读取 PA0、调用按键模块、发送按键事件并维持既有显示自检、心跳和 PA1 功能。`App_MotorTask()`拥有 ModeManager、PI、编码器故障和 PWM，因此不存在跨任务同步模式状态或故障状态的问题。

## 5. 按键识别状态机

`BSP_Key_t`保存原始电平、候选电平、稳定电平、候选开始时间、按下开始时间、第一次点击释放时间、第二次点击等待状态、长按已上报标志和上电释放保护标志。

处理规则：

1. 初始化时以 PA0 当前电平建立候选和稳定状态。
2. 如果上电时按键被按住，在稳定释放前不产生任何事件。
3. 原始电平变化时重置候选开始时间；候选电平连续保持 40 ms 后才更新稳定电平。
4. 稳定按下时记录按下时间。
5. 持续按下达到 1000 ms 时上报一次 `LONG`，清除待确认点击，并设置长按已上报标志。
6. 未触发长按就稳定释放时记为一次点击；第一次点击进入 350 ms 等待窗口。
7. 第二次按下在等待窗口内开始时进入双击候选；第二次稳定释放时上报一次`DOUBLE`，并取消`SHORT`。
8. 等待窗口结束且没有第二次点击时上报一次 `SHORT`。
9. 第二次按下演变成长按时，上报 `LONG`并取消第一次待确认点击。
10. 一次 `BSP_Key_Process()`调用最多返回一个事件。

按键模块接收调用方传入的原始按下状态和当前毫秒时间，不直接依赖 FreeRTOS 延时或阻塞等待，便于使用确定性时间序列自检。

## 6. 模式转换规则

上电或 RST 后固定初始化为：

```text
STANDBY + AUTO + LOW预选 + NONE故障
```

| 当前状态 | 事件 | 结果 |
| --- | --- | --- |
| STANDBY、无故障 | SHORT | `AUTO → MANUAL → BACKFLOW → AUTO`，电机保持停止 |
| STANDBY + MANUAL | DOUBLE | `LOW ↔ HIGH`预选，电机保持停止 |
| STANDBY、无故障 | LONG | 进入 RUNNING |
| RUNNING + AUTO | SHORT | 进入 MANUAL，按预选挡位启动 |
| RUNNING + MANUAL | SHORT | 进入 BACKFLOW，立即停止 |
| RUNNING + BACKFLOW | SHORT | 进入 AUTO，继续停止 |
| RUNNING/STANDBY + MANUAL | DOUBLE | `LOW ↔ HIGH`；RUNNING时重新软启动 |
| AUTO 或 BACKFLOW | DOUBLE | 忽略，不改变预选挡位 |
| RUNNING、无故障 | LONG | 进入 STANDBY，立即停止 |
| 任意故障状态 | SHORT/DOUBLE | 忽略，不允许重新启动 |
| 任意故障状态 | LONG | 清除故障并重置为`STANDBY + AUTO + LOW` |

“RUNNING”只表示系统运行许可，不保证电机正在旋转。M7 中 RUNNING + AUTO 和 RUNNING + BACKFLOW 仍输出停止。

## 7. MotorTask映射

MotorTask根据ModeManager快照计算电机需求：

```text
RUNNING + MANUAL + LOW  + NONE故障 → LOW_START → LOW_PI
RUNNING + MANUAL + HIGH + NONE故障 → HIGH_START → HIGH_PI
其余任意组合                    → STOP
存在故障                        → FAULT
```

- 从停止需求进入 MANUAL 时，先以 30% PWM 软启动 300 ms，再进入相应 PI 状态。
- 手动低/高档切换时重置 PI 积分，并重新执行软启动。
- 切换到 AUTO、BACKFLOW 或 STANDBY 时立即停止，清零 PI 积分和无反馈计数。
- 编码器连续 10 个 50 ms 周期没有有效反馈时，立即停止并把 ModeManager 和电机状态都置为 `ENCODER_TIMEOUT`。
- 故障中只有 `LONG`可以清除故障；清除后保持停止，必须再次长按取得运行许可并短按进入 MANUAL 才能启动。
- MotorTask每个50 ms周期取完队列内当前待处理事件并按顺序处理，再执行一次控制计算。

## 8. 日志与异常处理

模式或挡位实际变化时输出一次包含系统状态、模式、手动挡位和故障的日志。无效双击、故障中的无效短按/双击以及故障清除也输出明确原因。

DefaultTask使用0超时向事件队列发送，不阻塞心跳。队列满时保留已有事件、丢弃新事件并输出`key event queue full`。事件队列长度保持4；正常人手操作不会在一个50 ms控制周期内产生4个已消抖事件。

按键自检或模式管理自检失败时，MotorTask不允许启动电机，调用`BSP_Motor_Stop()`并周期输出失败信息。

## 9. 测试与验收

### 9.1 软件自检

按键固定时间序列覆盖：

- 40 ms以内的按下和释放抖动不产生事件。
- 单击在350 ms窗口结束后只产生一次`SHORT`。
- 双击只产生一次`DOUBLE`，不夹带`SHORT`。
- 长按达到1000 ms只产生一次`LONG`，释放不产生短按。
- 上电按住时不产生事件，稳定释放后才开始识别。
- 32位Tick回绕前后仍能正确判断消抖、双击和长按时间。

模式管理固定转换表覆盖全部有效和无效组合，重点验证AUTO/BACKFLOW输出停止、MANUAL挡位切换、故障锁存以及长按清故障重置。

### 9.2 无VM板端验收

断开TB6612 VM后验证：

1. 上电日志为`STANDBY AUTO LOW`，电机输出保持停止。
2. 短按每次只循环一个模式。
3. MANUAL待机下双击每次只切换一个预选挡位。
4. 长按只切换一次运行许可，释放不产生附加短按。
5. RUNNING进入MANUAL后因无反馈进入`ENCODER_TIMEOUT`。
6. 故障中短按和双击均无效。
7. 长按清故障后回到`STANDBY AUTO LOW`且保持停止。

### 9.3 接通VM空载验收

禁止安装扇叶或机械负载。完全断电后恢复VM，再验证：

1. 上电和STANDBY期间电机不转。
2. RUNNING + MANUAL默认低档稳定运行。
3. 双击切换高档且只切换一次。
4. 短按切出MANUAL后立即停止。
5. 长按进入STANDBY后立即停止。
6. RST后恢复`STANDBY AUTO LOW`，电机不自行启动。
7. DHT11、heartbeat、PA1、USART1和ST7735S无回归。

M7不主动执行编码器运行中断线测试；该项只有在用户后续明确要求时才执行，并继续遵守完全断电后接线、禁止运行中插拔电机电源线的安全规则。
