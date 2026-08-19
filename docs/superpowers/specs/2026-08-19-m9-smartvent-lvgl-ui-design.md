# M9 SmartVent LVGL中文状态面板设计

## 1. 目标与定位

M9为现有STM32F407控制固件增加正式的单页中文状态面板、显示故障提示和稳定性验证。项目对外名称统一为“智能通风边缘控制系统”，英文名称为`SmartVent Edge Controller`，界面简称为`SmartVent`。

本阶段只修改对外名称。CubeMX工程、Keil Target、固件目录和已有内部标识继续使用`SmartHood`，避免无功能收益的工程路径迁移。

M9使用LVGL 8.3.11构建128×160竖屏界面，但不把显示层加入控制闭环。MotorTask继续是唯一PWM所有者，UI故障不能改变电机状态。

## 2. 范围

M9包含：

- 移植LVGL 8.3.11并适配ARMCC5；
- 在现有ST7735S RGB565矩形写入接口上建立显示端口；
- 新增独立UiTask和线程安全UI快照；
- 实现单页精简中文状态面板；
- 显示DHT11过期、编码器故障和防回流功能预留状态；
- 验证Flash、RAM、FreeRTOS堆、任务栈和刷新耗时；
- 完成VM断开、VM接通空载及2小时稳定性验收。

M9不包含：

- MQ-2显示、采集或控制融合；
- 真实气流传感器和真实防回流判断；
- 未经标定的RPM显示或转速精度声明；
- 扇叶、机械负载、堵转或安规测试；
- 动画、阴影、大图片、完整中文字库或触摸交互；
- SPI DMA首版实现；
- 内部`SmartHood`工程和目录的批量重命名。

## 3. 已确认硬件与基线

显示硬件保持M2已经验收的连接：

| 信号 | STM32引脚 | 说明 |
|---|---|---|
| SPI2 SCK | PB13 | 5.25 MHz，模式0 |
| SPI2 MOSI | PB15 | 单向发送 |
| TFT CS | PD7 | 低电平有效 |
| TFT DC | PD6 | 命令/数据选择 |
| TFT RST | PD5 | 低电平复位 |
| TFT BLK | PD4 | 高电平点亮 |
| TFT VDD | 3.3V | 禁止接5V |
| TFT GND | GND | 与主控共地 |

屏幕为ST7735S，竖屏128×160，RGB565，已验证方向值`0xC0`、X/Y偏移均为0。现有BSP已经提供：

```c
bool BSP_ST7735S_SetAddressWindow(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height);

bool BSP_ST7735S_WritePixels(const uint16_t *pixels,
                             uint32_t count);
```

M8A正式构建基线为Code 29286字节、RO-data 1610字节、RW-data 172字节、ZI-data 39684字节，FreeRTOS总堆为32768字节。M9必须重新测量这些数据，不能沿用基线值作为最终结果。

## 4. 总体架构

采用“共享快照 + 独立UiTask”方案。MotorTask已经读取DHT11线程安全快照用于AUTO策略，因此由MotorTask统一发布一帧控制与传感器状态，避免UiTask自行拼接两个不同时间点的数据：

```text
SensorTask ──> SensorSnapshot ──> MotorTask ──> UiSnapshot
                                             │
                                             └──> UiTask ──> LVGL ──> ST7735S
DefaultTask ──> KeyEvent ──> MotorTask
```

任务职责：

- DefaultTask：20 ms扫描PA0，产生按键事件，并维持心跳和PA1指示灯；
- SensorTask：每2秒读取DHT11并发布通过校验的温湿度快照；
- MotorTask：每50 ms执行模式管理、自动策略、编码器采样、PI、PWM和故障锁存，并非阻塞发布完整UiSnapshot；
- UiTask：运行LVGL定时处理、复制UI快照并刷新ST7735S。

约束：

- 只有MotorTask可以初始化TB6612、修改PWM或改变电机内部状态；
- 只有UiTask可以调用LVGL和运行期ST7735S绘图接口；
- SensorTask和MotorTask不得调用LVGL；
- UiTask不向ModeManager、AutoPolicy、PI或MotorTask写入控制请求；
- UI读取失败、刷新失败或任务停止均不得改变电机输出。

## 5. UI快照模型

应用层增加一个面向显示的值快照：

```c
typedef struct
{
    ModeRunState_t run_state;
    ModeType_t mode;
    ModeManualLevel_t manual_level;
    ModeFault_t fault;

    int16_t temperature_x10;
    uint16_t humidity_x10;
    bool sensor_valid;
    uint32_t sensor_age_ms;

    int32_t target_count;
    int32_t feedback_count;
    uint8_t pwm_percent;
    bool motor_running;
} UiSnapshot_t;
```

`target_count`和`feedback_count`的含义是每个50 ms控制窗口的编码器计数，不是RPM。`feedback_count`使用与PI相同的绝对计数平均值，避免界面值与串口控制日志口径不一致。

快照发布规则：

1. SensorTask保留现有DHT11快照发布逻辑，不直接访问UI模块。
2. MotorTask每50 ms读取DHT11快照、完成控制计算后组装并尝试发布完整UiSnapshot。
3. 反馈计数只在500 ms统计窗口完成时替换为新的平均值，其余周期沿用最近一次完整平均值。
4. UI状态模块使用0超时互斥量保护整帧复制；MotorTask发布失败时直接跳过本帧，禁止为显示阻塞控制周期。
5. UiTask读取失败时保留上一次完整快照，并把本次显示状态标记为数据不可用；不得拼接新旧半帧数据。
6. DHT11快照年龄超过6000 ms时，MotorTask在UiSnapshot中写入`sensor_valid=false`及实际年龄；AutoPolicy继续按现有同一时间戳独立执行安全判断。

建议新增职责清晰的文件：

```text
App/Inc/ui_state.h        UI快照类型、初始化、非阻塞发布和复制接口
App/Src/ui_state.c        单生产者、单消费者的互斥同步实现
App/Inc/ui_screen.h       单页控件创建和快照应用接口
App/Src/ui_screen.c       中文文本、颜色和控件更新
App/Inc/lv_port_disp.h    LVGL显示端口公开接口
App/Src/lv_port_disp.c    ST7735S刷新回调和显示缓冲区
```

现有`app_tasks.c`只保留任务编排和生产者调用，避免把LVGL控件实现继续堆入该文件。

## 6. UiTask时序

UiTask配置：

```text
优先级：osPriorityBelowNormal
初始栈：512 Words / 2048 bytes
LVGL处理检查周期：5 ms
UI数据应用周期：500 ms
```

LVGL时间基准使用`HAL_GetTick()`或等价的单调毫秒Tick，不能简单假设每次任务循环都恰好经过5 ms。UiTask每轮调用`lv_timer_handler()`，阻塞刷新导致任务推迟时，LVGL仍依据真实Tick计算时间。

UiTask启动顺序：

1. 初始化ST7735S，失败时关闭背光并记录日志；
2. 初始化LVGL核心和显示端口；
3. 创建单页控件树；
4. 复制初始快照并完成首次绘制；
5. 进入5 ms定时处理循环；
6. 每500 ms复制并应用一次UI快照；
7. 仅修改内容或样式发生变化的控件。

M9启用正式UI后删除DefaultTask中的红、绿、蓝、白、黑启动自检。显示硬件自检保留为条件编译的临时测试入口，正式固件开关为`0U`。

## 7. LVGL显示端口

固定配置：

```text
LVGL：8.3.11
颜色深度：16 bit
LV_COLOR_16_SWAP：0
分辨率：128 × 160
刷新缓冲区：128 × 20个lv_color_t
缓冲区字节数：5120
缓冲方式：单缓冲
首版传输：SPI2阻塞发送
```

`LV_COLOR_16_SWAP=0`的原因是现有`BSP_ST7735S_WritePixels()`接收主控端RGB565数值，并主动按高字节、低字节顺序发送。LVGL端不再预交换，否则会造成字节颠倒。

刷新回调流程：

```text
LVGL脏矩形
  -> 裁剪到(0,0)-(127,159)
  -> BSP_ST7735S_SetAddressWindow()
  -> BSP_ST7735S_WritePixels()
  -> lv_disp_flush_ready()
```

无论BSP返回成功或失败，回调最终都必须调用`lv_disp_flush_ready()`，避免LVGL永久停留在刷新忙状态。失败同时发布显示诊断状态并限频输出日志，防止串口刷屏。

首版不启用SPI DMA。满足以下任一条件时，M9再增加DMA子阶段：

- 心跳或PA0扫描出现可重复的明显延迟；
- 50 ms MotorTask周期发生可观察抖动；
- 500 ms UI更新不能稳定完成；
- 测得的单次局部刷新阻塞时间影响控制验收。

DMA升级不得改变`ui_screen`接口或快照结构，只替换`lv_port_disp`与BSP传输实现。

## 8. LVGL内存与功能裁剪

LVGL首版配置`LV_MEM_SIZE = 16U * 1024U`静态内存池，不使用完整帧缓冲。首版只启用标签、基础容器和样式所需功能，关闭动画、阴影、大图片、透明特效、主题演示、小部件示例和未使用字体。若确定性控件创建测试证明16 KiB不足，必须先记录失败时的剩余内存和对象数量，再单独评审调整，不能静默扩大。

资源预算与验收项：

- 显示缓冲区固定5120字节；
- UiTask初始栈2048字节；
- 记录`uxTaskGetStackHighWaterMark()`得到的UiTask最小剩余栈；
- 记录FreeRTOS剩余堆及历史最小剩余堆；
- 记录Keil构建的Code、RO-data、RW-data和ZI-data；
- 如果任务创建失败或堆余量不足，先裁剪LVGL功能和内存池，再评估增大`configTOTAL_HEAP_SIZE`，禁止仅凭猜测扩大堆。

## 9. 字体与开源资源

界面使用两类字体：

- 英文、数字和单位：LVGL内置Montserrat精简字体；
- 中文：使用`NotoSansSC-Regular`字体源生成14像素、2 bpp精简字模，只包含界面实际字符；该字体采用SIL Open Font License 1.1。

精简中文字符集合覆盖以下文本：

```text
运行 待机 故障 自动模式 手动模式 防回流 预留
低档 高档 停止 温度 湿度 目标计数 反馈计数
输出 系统正常 传感器失联 编码器故障 功能预留
数据不可用
```

字体生成参数、`NotoSansSC-Regular`源文件版本、OFL-1.1许可证和转换命令必须记录在仓库中。固件只编译生成后的字模，不加载完整中文字库。首版不使用图片图标，以文字和色块表达状态。

## 10. 单页界面

采用已经确认的A“分区仪表盘”布局，包含四个区域。

### 10.1 顶部标题栏

```text
SmartVent       运行
```

左侧固定显示`SmartVent`，右侧显示`运行`、`待机`或`故障`。

### 10.2 模式与档位栏

```text
自动模式        低档
```

模式文本为`自动模式`、`手动模式`或`防回流·预留`。挡位文本为`低档`、`高档`或`停止`。待机、防回流和故障状态均显示`停止`。

### 10.3 环境数据区

```text
温度       湿度
27.1°C     64.0%
```

DHT11无有效或新鲜快照时，温度和湿度均显示`--`。数值使用扩大10倍的整数格式化，不引入浮点`printf`。

### 10.4 控制数据区

```text
目标计数    130
反馈计数    128
PWM输出     52%
```

STOP和FAULT时目标计数及PWM显示0；反馈计数显示最近一个完整控制窗口的测量值。界面不显示`RPM`或“转速”字样。

### 10.5 底部状态栏

状态优先级从高到低为：

1. 编码器故障：红色，`编码器故障`；
2. UI快照复制失败：黄色，`数据不可用`；
3. DHT11数据过期：黄色，`传感器失联`；
4. 防回流模式：橙色，`防回流·功能预留`；
5. 其他状态：灰蓝色，`系统正常`。

编码器故障时顶部状态同步显示`故障`。DHT11过期不会锁存ModeManager故障，AUTO由现有安全策略请求STOP，MANUAL仍由现有模式规则运行；UI只准确显示状态，不新增控制规则。

颜色语义：

```text
运行或有效值：绿色
温度：黄色
湿度：青色
待机：灰色
功能预留：橙色
故障：红色
```

## 11. 错误处理与隔离

- ST7735S初始化失败：关闭背光、记录一次错误，UiTask进入低频重试或诊断等待；MotorTask不受影响。
- UI快照复制失败：保留上一完整帧并标记数据不可用，不读取半更新结构体。
- DHT11超过6000 ms：温湿度显示`--`，状态栏显示传感器失联。
- 编码器超时：ModeManager继续锁存故障并停止电机，UI显示红色故障原因。
- LVGL刷新失败：必须释放刷新完成状态，限频记录日志，下一周期继续尝试。
- 非法枚举或越界数值：使用安全文本`--`或`故障`，禁止数组越界和未经检查的索引。
- UiTask不得持有UI快照锁执行SPI发送，避免生产者被整次刷新阻塞。

## 12. 测试顺序

### 12.1 软件与构建

1. 用纯数据快照验证全部文本映射和状态优先级。
2. 把LVGL、字体、端口和UiTask加入Keil工程。
3. ARMCC5完整Rebuild，要求0错误、0警告。
4. 确认正式自检开关全部为`0U`。
5. 记录Flash、RAM、FreeRTOS堆、UiTask栈高水位和刷新耗时。

### 12.2 VM断开验收

1. 断开VM，只连接已确认的主控、TFT、DHT11、ST-Link和串口。
2. 上电直接显示SmartVent界面，不再播放旧颜色自检。
3. 核对温湿度、模式、档位、目标/反馈计数和PWM。
4. 验证PA0短按、双击和长按后的界面在500 ms内更新。
5. 在完全断电后断开DHT11，重新上电并等待超过6000 ms，验证黄色`传感器失联`。
6. 完全断电后恢复DHT11，验证界面自动恢复。
7. 进入防回流模式，验证`功能预留`且电机请求为STOP。
8. 连续运行至少10分钟，确认无白屏、花屏、复位、串口失联或按键漏识别。

### 12.3 VM接通空载回归

仍然禁止扇叶、机械负载和堵转：

1. 上电保持STANDBY和PWM 0。
2. 验证AUTO、MANUAL低档和高档的屏幕状态与串口日志一致。
3. 验证长按、BACKFLOW和RST均安全停止。
4. 编码器断线测试只能断电改线后重新上电；故障锁存时显示红色`编码器故障`。
5. 核对UI刷新期间电机无明显顿挫，PA0无漏识别。

### 12.4 稳定性

1. 先连续运行30分钟并检查所有日志与资源余量。
2. 30分钟无异常后再执行2小时稳定性测试。
3. 记录心跳、DHT11恢复、控制日志、UI状态、UiTask栈高水位和历史最小剩余堆。
4. 记录任何花屏、白屏、异常复位、任务失联或控制周期异常。

## 13. 验收标准

- ARMCC5完整Rebuild为0错误、0警告；
- 单页中文界面字段、模式、故障和串口日志一致；
- PA0交互在500 ms内反映到界面且无控制回归；
- DHT11过期和恢复提示正确；
- 编码器故障锁存和红色提示正确；
- 防回流明确显示功能预留并保持STOP；
- UI刷新不造成可观察的电机顿挫、心跳中断或按键漏识别；
- 2小时运行无异常复位、白屏、花屏或任务失联；
- 最终文档记录资源占用、测试结果和全部未执行限制。

## 14. 对外展示边界

README和界面使用“智能通风边缘控制系统 / SmartVent Edge Controller”。对外描述可以强调FreeRTOS多任务、线程安全快照、闭环控制、故障锁存、自动策略和LVGL界面，但不得声称：

- 已达到量产或工业认证要求；
- 已完成EMC、安规或长期老化；
- 已获得可靠RPM精度；
- 已实现MQ-2烟雾浓度测量；
- 已实现真实防回流检测；
- 已通过扇叶、负载、堵转或整机温升测试。

更准确的项目定位是“面向工程化的嵌入式智能通风边缘控制原型”。
