# STM32F407 智能通风边缘控制系统项目指南

## 当前状态

- 当前阶段：M8A DHT11自动策略已完成；M9 SmartVent LVGL中文状态面板设计已确认，等待用户复核正式设计文档后生成逐步实施计划。
- 当前工程：`firmware/SmartHood`已创建，可由Keil MDK-ARM编译。
- 对外名称：智能通风边缘控制系统 / SmartVent Edge Controller；内部CubeMX工程、Keil Target和目录继续使用`SmartHood`。
- 开发路线：分层增量开发。
- 第一阶段范围：主控功能，不包含Bootloader。

## 项目目标

基于 STM32F407VET6 和 FreeRTOS，实现传感器采集、单键交互、ST7735S中文状态显示、直流电机编码器反馈、PI闭环、自动调速、手动档位、故障锁存和可扩展的通风控制模式。

## 已确认关键参数

- 电机6V空载标称转速：300 RPM；M4使用5V VM时预期低于该数值。
- 电机型号：JGA12-N20-50B，6V额定，减速比50，空载40mA，堵转0.55A。
- 编码器产品资料标称AB双相、7 PPR和3.3V供电，理论四倍频为1400计数/输出轴圈；当前3.3V供电、内部上拉和固定正转方向关系已通过功能验证，C1/C2具体输出电路类型仍未确认。
- 手动目标转速：190 RPM和220 RPM。
- PID/测速周期：50 ms。
- 电机PWM目标频率：20 kHz。
- TFT：ST7735S，SPI接口。
- TFT实物：1.8英寸128×160，丝印顺序BLK/CS/DC/RST/SDA/SCL/VDD/GND。
- 用户输入：PA0单按键；RST仅复位。
- DHT11读取周期：2 s。
- MQ-2：第一阶段使用ADC值或相对值，不直接标注ppm。
- MQ-2模块：LM393 Flying-Fish模块，5V供电；商家手册明确AO为0~5V，必须分压后接STM32 ADC。
- 电源模块：3A DC-DC多路模块，输入6～30V，提供VIN直通、5V、3.3V和GND。
- 不使用开发板P2板载TFT插座；ST7735S通过杜邦线连接SPI2和独立GPIO。
- 板载W25QXX使用SPI1重映射引脚PB3/PB4/PB5，片选PA15。
- PA0板载按键按下为高电平，软件配置内部下拉并使用轮询。

## 里程碑进度

| 里程碑 | 内容 | 状态 |
|---|---|---|
| M1 | CubeMX、时钟、串口、FreeRTOS最小工程 | 功能通过：10分钟稳定性未测试 |
| M2 | ST7735S显示 | 通过：颜色、方向、边框、文字及M1回归功能正常 |
| M3 | DHT11、MQ-2采集与滤波 | M3A DHT11全部通过；M3B MQ-2等待测量与分压条件 |
| M4 | TB6612、电机开环PWM | 功能通过：开环挡位、停止、长按、RST安全及系统回归正常；照片核对豁免 |
| M5 | 编码器测速与标定 | 功能通过：接线、回绕和三挡测速正常；实际CPR标定跳过 |
| M6 | 相对速度PI闭环 | 功能通过：低档闭环、无反馈故障锁存、人工清除、停止与RST安全正常；高档主观运行正常，实际断线测试跳过 |
| M7 | 单键交互与模式管理 | 通过：板端自检、真实按键交互、模式和故障联动均已验证 |
| M8 | 自动融合与防回流 | M8A DHT11 AUTO空载通过；MQ-2/BACKFLOW待完成 |
| M9 | UI、异常处理、稳定性测试 | 设计已确认：LVGL 8.3.11、单页中文面板、独立UiTask与共享快照；待实施计划 |
| M10 | 参数、文档和简历数据整理 | 未开始 |

## 开发记录

后续每次开发在这里追加：

1. 本次目标。
2. CubeMX配置变化。
3. 新增或修改的文件。
4. 编译结果。
5. 硬件测试结果。
6. 遇到的问题和解决方式。
7. 下一步。

### 2026-07-20：参数资料核对

- 确认ST7735S为1.8英寸128×160，接口为BLK/CS/DC/RST/SDA/SCL/VDD/GND。
- 确认MQ-2为带LM393的Flying-Fish模块，5V供电，AO接ADC前必须分压。
- 确认电机为JGA12-N20-50B：6V、300 RPM、减速比50、堵转电流0.55A。
- 确认3A DC-DC模块输入6～30V，提供VIN直通、5V和3.3V输出。
- 确认用户具备ST-Link和USB转TTL。
- 下一步：M1，仅连接STM32、ST-Link和USB转TTL，创建CubeMX工程。

### 2026-07-20：M1开发环境确认

- STM32CubeMX版本：6.16.1。
- STM32CubeF4固件包版本：1.28.3。
- 编译工具：Keil MDK-ARM V5，具体小版本待首次打开工程后记录。
- 状态：开发环境通过，准备创建STM32F407VETx工程。

### 2026-07-21：M1 MCU选择

- CubeMX已进入STM32F407VETx、LQFP100引脚配置界面。
- MCU选择检查通过，尚未配置引脚和时钟。
- 下一步：设置SmartHood项目名称、保存路径和MDK-ARM V5工具链。

### 2026-07-21：M1工程参数

- 工程名：SmartHood。
- 保存位置：`D:\Keil5 prj\stm32f4\firmware`。
- 工具链：MDK-ARM，CubeMX最低版本选项V5.32。
- Application Structure：Advanced。
- 链接器最小Heap/Stack：0x400/0x800。
- 工程参数截图检查通过，尚未生成代码。

### 2026-07-21：M1时钟配置说明

- HSE使用板载8MHz晶振，主PLL配置为M=8、N=336、P=2，系统主频168MHz。
- M1没有启用USB OTG FS、SDIO或RNG，因此CubeMX会锁定PLLQ选项；PLLQ不影响168MHz系统主频。
- 后续如果启用需要48MHz时钟域的外设，再将PLLQ设置为7。
- 实际配置截图已确认：HSE=8MHz，M/N/P=8/336/2，SYSCLK/HCLK=168MHz，APB1=42MHz，APB2=84MHz。

### 2026-07-21：M1基础GPIO

- PA1配置为BOARD_LED推挽输出，初始高电平、无上下拉、低速。
- PA0配置为USER_KEY输入并启用内部下拉；按键松开预期为0，按下预期为1。

### 2026-07-21：M1调试串口

- USART1使用PA9 TX和PA10 RX，异步模式。
- 参数为115200、8N1、收发双向、16倍过采样。
- M1使用阻塞发送日志，不启用USART1中断和DMA。

### 2026-07-21：M1 FreeRTOS接口

- FreeRTOS接口选择CMSIS_V2。
- 固件包内FreeRTOS版本10.3.1，CMSIS-RTOS版本2.1.3。
- HAL使用TIM6、FreeRTOS使用SysTick，启用时未出现时基冲突。
- FreeRTOS动态堆配置为32768 Bytes。
- defaultTask使用osPriorityNormal、256 Words栈、动态分配，入口StartDefaultTask。

### 2026-07-21：M1代码生成策略

- 只复制工程所需的库文件。
- 每个外设分别生成`.c/.h`初始化文件。
- 重新生成时保留USER CODE区域。
- 允许CubeMX删除不再生成的旧生成文件，手写模块必须放在独立目录或USER CODE区域。

### 2026-07-21：Keil设备包阻塞

- CubeMX已生成SmartHood的MDK-ARM工程。
- Keil当前只有STM32F1设备包，缺少STM32F4设备支持。
- 需要通过Keil Pack Installer安装`Keil::STM32F4xx_DFP`后再进行首次编译。
- 进一步确认当前MDK为5.24a、PackUnzip为1.2.18，无法兼容2025版DFP 3.1.1的Pack Schema 1.7.36和新版许可证结构。
- 解决方案调整为升级MDK到5.32或更高版本，不采用管理员权限或手工解压规避格式不兼容。
- 用户暂时采用旧版兼容方案并安装DFP 1.0.8；Keil中需将目标器件从CubeMX名称STM32F407VETx映射为旧包名称STM32F407VE。
- 此方案用于推进M1，长期仍建议升级MDK和DFP。
- Keil目标器件映射已完成，原始CubeMX工程首次编译为0错误、0警告。
- 首次编译大小：Code 11348、RO-data 496、RW-data 148、ZI-data 39364。

### 2026-07-22：M1串口日志模块

- 创建`App/Inc/debug_log.h`和`App/Src/debug_log.c`。
- `DebugLog_Printf`使用长度受限的`vsnprintf`格式化日志，再通过USART1阻塞发送。
- 模块已加入Keil并通过0错误、0警告编译；当前没有调用者，因此链接器尚未把函数放入最终镜像。

### 2026-07-22：M1应用任务模块

- 创建`App/Inc/app_tasks.h`和`App/Src/app_tasks.c`。
- 应用任务负责每秒翻转PA1、读取PA0，并通过USART1输出心跳、按键值和HAL Tick。
- `app_tasks.c`已加入Keil的App分组并独立通过0错误、0警告编译。
- `freertos.c`已在USER CODE区域包含`app_tasks.h`，并由`StartDefaultTask`调用`App_DefaultTask(argument)`。
- 集成后编译结果：0错误、0警告，Code=18416、RO-data=1048、RW-data=148、ZI-data=39364。
- 程序大小增加说明应用任务、格式化日志和USART1发送代码已经进入最终镜像。

### 2026-07-23：M1首次上板运行

- ST-Link通过SWD识别STM32F407，IDCODE为`0x2BA01477`，调试时钟4MHz。
- 旧DFP 1.0.8按其设备映射使用`STM32F4xx Flash` 1MB通用下载算法，工程IROM仍为512KB。
- USART1已在115200、8N1下输出连续心跳，心跳序号和HAL Tick增长正常。
- PA0按键已验证：松开为0、按下为1。
- PA1板载LED已验证按每秒一次翻转。
- 10分钟连续运行验收本轮按用户决定跳过，保留为未测试项。
- M1结论：编译、下载、FreeRTOS、USART1、PA0和PA1基础功能通过。

### 2026-07-23：M2 ST7735S设计确认

- 采用竖屏128×160、RGB565和HAL阻塞式SPI2发送方案。
- SPI2规划为PB13 SCK、PB15 MOSI；PD7/PD6/PD5/PD4分别控制CS/DC/RST/BLK。
- 驱动和ASCII字模放入独立BSP目录，M2不使用DMA、不创建UiTask。
- M2底层接口预留RGB565矩形连续写入能力，LVGL移植安排在M9正式UI阶段。
- 设计文档已完成，下一步在用户审核后生成CubeMX与Keil分步实施计划。
- M2详细实施计划已生成，包含CubeMX、BSP驱动、接线、显示校准、Git检查点和M9 LVGL预留接口。

### 2026-07-25：M2实施启动

- 已在`feature/m2-st7735s`分支开始M2，开始前工作区干净。
- M1基线工程在Keil中重新编译通过，无错误；M1代码保护点保持不变。
- 当前检查点为CubeMX配置SPI2和PD4-PD7，在截图核对前不生成代码、不连接TFT。
- CubeMX配置及生成结果已核对：SPI2使用PB13 SCK、PB15 MOSI、5.25MHz、模式0；PD4-PD7的标签和初始电平正确。
- `freertos.c`中的`app_tasks.h`和`App_DefaultTask(argument)`均未被重新生成覆盖。
- CubeMX将Keil目标器件名称重新写为`STM32F407VETx`；预案是在旧DFP再次报告设备错误时，才在Keil中重新映射为`STM32F407VE`。
- 实际空外设全量编译在`STM32F407VETx`名称和DFP 1.0.8组合下通过，因此本轮未强制重新映射；原`STM32F4xx_1024`下载算法保持不变。
- 空外设编译结果：0错误、0警告，Code=18868、RO-data=1048、RW-data=148、ZI-data=39452。
- BSP目录、Keil BSP分组和`..\BSP\Inc`包含路径已建立。
- M2最小5x7 ASCII字模模块`fonts.h/.c`已加入Keil并参与编译；结果为0错误、0警告。
- ST7735S阻塞式BSP驱动已实现：初始化、背光、地址窗口、RGB565连续写入、矩形/像素填充和最小文本绘制接口均已完成。
- `fonts.c`和`bsp_st7735s.c`均已加入Keil BSP分组；完整BSP构建为0错误、0警告。由于应用层尚未引用驱动，链接程序大小仍为Code=18866、RO-data=1050、RW-data=148、ZI-data=39452。

### 2026-07-26：M2显示自检集成

- `App_DefaultTask`启动时调用一次ST7735S显示自检，之后继续运行原M1心跳、PA0读取和PA1翻转循环。
- 自检包含红、绿、蓝、白、黑全屏切换，四边白框、四色角标以及三行英文/数字文本。
- 集成构建结果：0错误、0警告，Code=21234、RO-data=1238、RW-data=148、ZI-data=39452。
- 当前固件已达到断电接线前检查点，尚未连接TFT、烧录或校准方向/颜色/偏移。
- 首次上板后主任务和串口心跳正常，但ST7735S仅表现为复位时先黑后白并持续白屏；当前正在区分通信接线、SPI速度和初始化兼容问题，M2尚未通过。
- 重新接线后白屏消失，画面方向、文字和四边坐标正常；初始MADCTL 0xC8导致红蓝通道互换，正在以0xC0进行单变量颜色校准。
- 最终显示校准值：MADCTL=0xC0、X Offset=0、Y Offset=0、背光高电平有效；颜色、竖屏方向、四角、四边和测试文字均正确。
- 照片中沿单像素白边出现的固定彩色细纹为RGB子像素成像效果，不属于随机SPI杂点。
- PA0按下/松开、PA1每秒翻转、USART1心跳、显示稳定性、发热和异常重启回归检查均通过。
- M2结论：功能通过。保留HAL阻塞式SPI2与5.25MHz配置，不启用DMA或LVGL；项目在此检查点停止，等待M3。

### 2026-07-26：M3A DHT11设计确认

- M3拆分为M3A DHT11与M3B MQ-2；当前先实施DHT11，MQ-2等待万用表、10 kΩ/15 kΩ分压电阻和面包板。
- DHT11采用3.3V供电，DATA规划为PD0；TIM5配置为1 MHz、32位自由运行微秒计数器，不启用中断。
- 新建独立SensorTask，每2秒读取一次DHT11；M3A只输出USART1日志，不修改TFT画面。
- 驱动返回OK、TIMEOUT或CHECKSUM_ERROR；所有等待均带超时，断线不能卡死，重新接入后无需复位即可自动恢复。
- 为支持默认任务和传感器任务共同输出日志，M3A将为DebugLog增加互斥保护。
- 详细设计见`docs/superpowers/specs/2026-07-26-dht11-acquisition-design.md`；当前只完成设计，尚未配置CubeMX、编写驱动或连接硬件。

### 2026-07-26：M3A DHT11实施计划

- 已将M3A拆分为基线检查、CubeMX配置、DebugLog互斥保护、DHT11 BSP、SensorTask集成、未连接测试、正常采集、断线恢复和最终合并等检查点。
- CubeMX关键值确定为PD0输入上拉、TIM5内部时钟、PSC=83、ARR=0xFFFFFFFF、无TIM5中断；sensorTask为Normal优先级、256 Words动态栈。
- 实施过程中由用户完成CubeMX、Keil、代码录入和硬件操作，助手逐步讲解、核对每个检查点并负责全部Git操作。
- 详细计划见`docs/superpowers/plans/2026-07-26-m3a-dht11-acquisition.md`；下一步从M2基线全量编译和M3A功能分支开始。

### 2026-07-26：M3A CubeMX基础配置

- 已在`codex/feature-m3a-dht11`分支启动M3A；开始前M2基线全量编译与最终记录完全一致：Code=21234、RO-data=1238、RW-data=148、ZI-data=39452，0错误、0警告。
- PD0已配置为`DHT11_DATA`输入并启用内部上拉，用于在传感器断线时保持确定高电平。
- TIM5已启用内部时钟，PSC=83、ARR=0xFFFFFFFF、向上计数、时钟不分频、自动重装预装载关闭、无TIM5中断；计数频率为1 MHz。
- FreeRTOS新增`sensorTask`：Normal优先级、256 Words动态栈、入口`StartSensorTask`；当前仍为CubeMX默认空循环。
- CubeMX生成后，`tim.h/.c`、`MX_TIM5_Init()`、PD0 GPIO初始化和sensorTask均已核对；原`App_DefaultTask(argument)`委托、DFP 1.0.8与Flash下载算法保持有效。
- 空外设全量构建通过：Code=21886、RO-data=1286、RW-data=152、ZI-data=39528，0错误、0警告。

### 2026-07-27：M3A DebugLog多任务保护

- `DebugLog_Init()`使用CMSIS-RTOS2动态创建互斥量，并在`MX_FREERTOS_Init()`中、两个任务创建前完成初始化；创建失败时进入`Error_Handler()`。
- `DebugLog_Printf()`在格式化和USART1阻塞发送期间持有互斥量，所有正常出口均释放互斥量，避免defaultTask与后续sensorTask并发输出交叉。
- 集成构建通过：Code=23150、RO-data=1286、RW-data=156、ZI-data=39524，0错误、0警告。
- 烧录回归确认：启动日志、心跳、PA0按下/松开、PA1每秒翻转和ST7735S测试画面均正常，无重复启动或串口乱码。
- 当前sensorTask仍为空循环，尚未创建或调用DHT11 BSP。

### 2026-08-02：M3A DHT11 BSP驱动

- 新增`BSP/Inc/bsp_dht11.h`和`BSP/Src/bsp_dht11.c`，接口使用扩大10倍的整数表示温度与湿度，并区分OK、TIMEOUT和CHECKSUM_ERROR。
- 驱动使用TIM5自由运行计数器实现微秒延时和所有边沿超时；PD0在开漏输出与输入上拉之间动态切换。
- 主机启动信号为约18 ms低电平；释放总线后在保存并屏蔽中断的状态下等待30 μs，再采集DHT11响应和40位数据，避免任务切换干扰短脉冲测量。
- 所有采集失败路径均在返回前恢复进入读取前的中断状态和PD0输入模式；无无限等待循环。
- `bsp_dht11.c`已加入Keil BSP分组并参与全量编译；构建为0错误、0警告。应用层尚未调用驱动，因此程序大小仍为Code=23150、RO-data=1286、RW-data=156、ZI-data=39524。
- 当前尚未烧录DHT11采集逻辑或连接DHT11，下一步接入`App_SensorTask()`。

### 2026-08-02：M3A SensorTask集成

- `app_tasks.h`新增`App_SensorTask()`接口；CubeMX生成的`StartSensorTask()`已在USER CODE区域委托该应用任务。
- SensorTask启动TIM5，等待DHT11上电稳定2秒，随后每约2秒读取一次并输出OK、TIMEOUT或CHECKSUM_ERROR日志。
- 温湿度日志使用扩大10倍的整数拆分为一位小数，不启用浮点格式化；负温度保留符号处理。
- 首次全量构建发现`app_tasks.c`末行缺少换行符，ARMCC报告1条格式警告；补充文件末尾换行后警告清零。
- 最终集成构建：Code=24002、RO-data=1286、RW-data=156、ZI-data=39524，0错误、0警告，Build Time 17秒。
- 当前固件尚未烧录测试，DHT11仍保持未连接；下一步验证未连接时稳定输出TIMEOUT且不影响M1/M2功能。

### 2026-08-02：自编代码中文注释规范确认

- 按用户要求，M3A未连接TIMEOUT测试暂缓，先为全部自编App/BSP代码和Core的USER CODE区域补充结构化中文注释。
- 不修改CubeMX生成区、HAL、LL、CMSIS、FreeRTOS或Keil设备包源码，避免生成覆盖和无关第三方改动。
- 注释重点覆盖模块职责、公开接口、任务流程、DHT11时序、ST7735S坐标/颜色、DebugLog互斥和错误恢复；不机械逐行翻译，不逐字节注释字模。
- 代码保持UTF-8，通过小文件试验验证Keil 5.24与ARMCC 5.06兼容后再批量修改。
- 注释改造只允许改变注释、空白和文件编码；验收要求Program Size保持Code=24002、RO-data=1286、RW-data=156、ZI-data=39524，并比较修改前后HEX哈希。
- 以后助手提供的自编代码默认包含中文注释，并在代码后说明实现流程、设计原因、关键参数和易错点。
- 详细设计见`docs/superpowers/specs/2026-08-02-chinese-code-commenting-design.md`。
- 分步实施计划见`docs/superpowers/plans/2026-08-02-chinese-code-comments.md`，按UTF-8试验、App、DHT11、ST7735S/字模、FreeRTOS USER CODE和零行为变化验收依次执行。
- UTF-8小文件试验已通过Keil显示与编译检查；随后按用户要求一次性完成其余App/BSP及`freertos.c` USER CODE中文注释。
- 静态审计将注释和空白剥离后与注释前基线逐文件比较，11个目标文件的有效代码全部一致。
- 最终Rebuild保持Code=24002、RO-data=1286、RW-data=156、ZI-data=39524，0错误、0警告；HEX SHA-256仍为`0DC8FC5B0E3CDCCAF4860CBD36F1DF81B97263A7F2E895ED10C4A2DDCA7C96E8`。
- 用户已确认烧录后的心跳、DHT11未连接TIMEOUT、PA0、PA1和ST7735S画面均正常，中文注释改造验收通过。

### 2026-08-03：M3A DHT11最终硬件验收

- 三针DHT11以3.3V供电，DATA接PD0，正常采集连续输出OK；截图样本温度30.8～31.3°C、常态湿度70%～71%。
- 缓慢哈气后湿度上升到87%，随后回落到70%，环境变化响应正常。
- 运行中只断开DATA线后稳定输出TIMEOUT；不按RST重新接回后自动恢复OK，完整断线恢复过程重复两次均通过。
- 全程heartbeat持续递增，PA0、PA1、USART1和ST7735S无回归；M3A结论为全部通过。
- M3B MQ-2仍禁止直接接入STM32，等待万用表、10 kΩ/15 kΩ电阻和面包板或可靠焊接转接；当前转入M4设计准备。

### 2026-08-04：M4开环电机控制设计与计划

- 使用TB6612FNG A通道驱动JGA12-N20-50B，M4只验证固定正转和0%、30%、50%、70%开环占空比，不连接编码器、不实现PID或软件反转。
- 供电确定为9V适配器进入DC-DC，DC-DC 5V连接TB6612 VM，STM32 3.3V连接VCC，STM32、DC-DC与TB6612共地；禁止把VIN直通接到VM。
- 控制引脚确定为PB6/TIM4_CH1/PWMA、PB7/AIN1、PB8/AIN2、PB9/STBY；上电默认STBY、PWM和方向信号均为低。
- 实际转接线确定为黑色M1接AO1、绿色M2接AO2；橙色VCC、黄色C2、白色C1和红色GND在M4全部不接并分别绝缘。
- PA0采用20ms轮询和40ms软件消抖，每次有效按下按“停止→30%→50%→70%→停止”循环；PA1和心跳继续保持约1秒周期。
- 详细设计见`docs/superpowers/specs/2026-08-03-m4-tb6612-open-loop-motor-design.md`，逐步实施计划见`docs/superpowers/plans/2026-08-04-m4-tb6612-open-loop-motor.md`。
- 当前只完成设计和计划，尚未修改CubeMX、固件或硬件接线；下一步先执行M3A基线全量编译。

### 2026-08-07：M4开环电机控制最终验收

- TIM4_CH1使用PB6输出20kHz PWM，PSC=0、ARR=4199、初始Pulse=0；PB7/PB8/PB9分别控制AIN1/AIN2/STBY，CubeMX初始输出全部为低。
- 新增`bsp_motor.h/.c`封装TB6612 A通道初始化、固定正转、占空比换算和安全停止；所有自编代码包含结构化中文注释。
- PA0使用20ms轮询和40ms消抖，挡位按停止、30%、50%、70%、停止循环；长按不连续跳挡。
- 无电机软件测试、分级上电、30%起转、50%/70%速度趋势、0%停止、RST停止和空载系统回归全部通过。
- 最终Rebuild：Code=25710、RO-data=1290、RW-data=160、ZI-data=39600，0 Error(s)、0 Warning(s)，Build Time 15秒。
- 最终HEX SHA-256：`950DDFECC1181A4EEA73A27C3AE9B8E1BA66B6B03680289E0A372C029BD991BF`。
- 限制：用户明确跳过助手接线照片核对；没有万用表，未实测DC-DC 5V或启动压降；未进行堵转、带扇叶、带机械负载或长时间温升测试。
- M4结论：所有已执行的功能与安全验收项目通过；助手接线照片核对按用户明确授权豁免、未执行。下一步进入M5编码器资料与电气接口设计，不直接沿用旧线色或开漏输出假设。

### 2026-08-15：M6相对速度PI闭环实施启动

- 已创建功能分支`codex/feature-m6-relative-pi`，从已完成M5的`main`开始。
- M6开始前Rebuild：Code=26986、RO-data=1338、RW-data=164、ZI-data=39668，0 Error(s)、0 Warning(s)，Build Time 30秒。
- 基线HEX SHA-256：`7E9543FC3A23014FC07FB54B5D550A6986F867067CF8B40C7C0B55F1B8F612E4`。
- 已建立`Control/Inc`、`Control/Src`和`Control/Test`，完成Q8定点PI、输出限幅、积分抗饱和和板端自检。
- PI自检串口输出`control PI self-test PASSED`，TFT、DHT11、heartbeat和编码器启动回归正常；正式构建已将临时自检开关恢复为`0U`。
- 已建立PA0到MotorTask的NEXT命令队列，DefaultTask不再初始化电机或直接修改PWM；队列在任务创建前完成初始化。
- MotorTask已实现STOP、低/高档软启动、相对计数PI闭环和编码器无反馈FAULT锁存，并成为唯一修改PWM的任务。
- VM隔离测试已验证上电STOP、无反馈ENCODER_TIMEOUT、FAULT持续锁存、第一次PA0清故障保持STOP及第二次PA0才重新启动。
- 低档空载闭环保持Kp=64/256、Ki=4/256，目标130 counts/50ms；样本actual为129至130、duty为48%至49%，运行约10秒稳定，无需调参。
- 高档空载运行由用户确认无异常，但未提供actual和duty数值截图，因此不记录精确高档计数验收结论。
- PA0停止与RST停止均通过；用户明确跳过C1/C2实际断线测试。仍禁止扇叶、机械负载、堵转和长时间温升测试。
- M6最终Rebuild为Code=28042、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，HEX SHA-256为`F85F226CE1B5F8F3FEBE85360801217742E2ED39B3CA8E6A53313A6F0016CA55`。

### 2026-08-16：M7单键交互与模式管理设计

- PA0继续使用20ms轮询和40ms消抖，新增350ms双击窗口及1000ms一次性长按识别；单击等待双击窗口结束后再确认。
- 交互确定为长按切换运行/待机、短按循环AUTO/MANUAL/BACKFLOW、双击仅在MANUAL切换LOW/HIGH。
- 上电固定为STANDBY + AUTO + LOW预选；M7中的AUTO和BACKFLOW只维护状态并保持电机停止，MANUAL复用M6相对计数闭环。
- DefaultTask只产生按键事件并发送队列；ModeManager由MotorTask调用，使模式、PI、编码器故障和PWM保持在同一任务内，MotorTask继续是唯一PWM所有者。
- 编码器故障中短按和双击无效；长按清故障并重置为STANDBY + AUTO + LOW，防止清除后立即重新启动。
- 设计包含按键时间序列自检、模式转换表自检、无VM故障交互验收和接通VM空载验收；不主动执行编码器运行中断线测试。
- 详细设计见`docs/superpowers/specs/2026-08-16-m7-single-key-mode-manager-design.md`；下一步由用户复核书面设计，再编写逐步实施计划。
- M7实现分支为`codex/feature-m7-key-mode`；开始实现前完整Rebuild为Code=28042、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)。
- M7基线HEX SHA-256为`F85F226CE1B5F8F3FEBE85360801217742E2ED39B3CA8E6A53313A6F0016CA55`，与M6最终固件一致。
- 按键识别模块已实现并通过板端确定性自检，覆盖消抖、单双击、长按、上电按住保护和Tick回绕。
- 模式管理器已实现并通过完整转换表自检，最终Rebuild为Code=30178、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)。
- VM断开时板端输出`M7 self-test PASSED`和`motor control ready, state=STOP`，后续控制日志持续为零输出、无故障；下一步进入Task 6，把真实PA0输入迁移为按键事件生产者。
- Task 6已把PA0输入迁移到`BSP_Key`识别器和`BSP_KeyEvent_t`消息队列，DefaultTask不再维护重复消抖状态；最终Rebuild为Code=30182、0 Error(s)、0 Warning(s)。
- Task 6只是不可烧录的中间构建，MotorTask尚未根据SHORT/DOUBLE/LONG区分行为；下一步必须直接执行Task 7，把事件交给ModeManager并删除旧M6单步切换。
- Task 7已把按键事件、ModeManager请求和M6软启动/PI/故障状态机完成软件集成；请求映射由同一纯函数同时服务生产路径和板端自检，非法请求安全停机。
- Task 7最终Rebuild为Code=30646、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)；尚未烧录或执行真实按键交互，下一步在VM断开条件下完成Task 8板端验收。
- Task 8在VM断开条件下验证了AUTO/MANUAL/BACKFLOW短按循环、MANUAL双击LOW/HIGH往返、长按一次性、无反馈ENCODER_TIMEOUT锁存、故障中事件屏蔽和长按安全清除；所有停止路径均保持PWM为0。
- Task 8期间heartbeat和DHT11正常，未观察到持续串口乱码或任务阻塞；临时M7自检开关在验收后恢复为`0U`，下一步进入接通VM的空载验收。
- 关闭临时自检后的正式Rebuild为Code=28778、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)；Task 8无VM验收完成。
- Task 9在无扇叶、无机械负载条件下完成接通VM空载验收：MANUAL低档和高档运行正常，BACKFLOW、长按STANDBY和RST三条停止路径均通过。
- 低档截图样本为actual 127至134、duty 49%至52%、fault=0；高档由用户确认运行正常但未提供精确计数截图，因此不记录数值范围。
- M7最终构建为Code=28778、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，HEX SHA-256为`EB81C1253245E82124DA8521D648EFC3183B8CEFEA7628E532ACD5AAB96AC8A4`。
- 编码器运行中断线、扇叶、机械负载、堵转和长时间温升仍未执行；M7结论不覆盖这些场景。

### 2026-08-18：MQ-2商家手册核对

- 通过Word文档读取`参数/产品使用手册.doc`，确认模块为LM393 + ZYMQ-2，工作电压5V。
- 手册明确写出“模拟量输出0~5V电压，浓度越高电压越高”，因此AO不能直接连接STM32F407的PC0 ADC输入。
- 手册称TTL输出低电平有效且可接单片机，但未给出高电平最大电压；当前阶段不使用DO，避免把未确认的5V高电平接入非5V容忍或模拟引脚。
- 由于当前仍没有万用表、10kΩ/15kΩ分压电阻和面包板，MQ-2 AO/DO继续保持断开，M8暂不开始硬件接入。

### 2026-08-19：M8A DHT11自动策略无VM验收

- 新增纯函数AutoPolicy、DHT11线程安全快照和ModeManager AUTO候选请求接口，MotorTask继续是唯一PWM所有者。
- AUTO初始阈值为28.0摄氏度或70%RH进入LOW、32.0摄氏度或85%RH进入HIGH；关闭阈值使用31.0摄氏度/80%RH和27.0摄氏度/65%RH形成迟滞。
- VM断开板端自检明确输出`M8A auto policy self-test PASSED`；真实DHT11从27.1摄氏度/64%RH的STOP，经湿度92%和98%进入HIGH，再在湿度79%和77%时降为LOW。
- STANDBY优先级、MANUAL挡位、BACKFLOW停止、模式循环和RST安全初值回归由用户确认无问题；整个无VM测试中控制状态保持STOP。
- 为避免带电插拔线路，DHT11运行中断线和真实LOW到STOP边界未执行；6000ms过期停止与全部数值边界由固定输入自检覆盖。
- 正式自检开关恢复为0U，完整Rebuild为Code=29286、RO-data=1610、RW-data=172、ZI-data=39684，0 Error(s)、0 Warning(s)，HEX SHA-256为`26F73313BC6786BE6570FA54ADCF3B7206D6D394317025BED18C7EB6082EDCD5`。
- 正式固件在无扇叶、无机械负载条件下完成VM接通空载验收：上电STANDBY不自启动，AUTO阈值触发后正常软启动，再次长按进入STANDBY立即停止，用户确认无异常。

### 2026-08-19：M9 SmartVent LVGL中文状态面板设计

- 项目对外名称改为“智能通风边缘控制系统 / SmartVent Edge Controller”，界面简称`SmartVent`；内部`SmartHood`工程、Keil Target和目录保持不变。
- M9采用LVGL 8.3.11、128×160竖屏、RGB565、128×20像素单缓冲和首版SPI2阻塞发送；出现可重复调度影响时才升级DMA。
- 采用共享快照与独立UiTask架构；UiTask只读系统状态并刷新界面，MotorTask继续是唯一PWM所有者。
- 单页A布局显示运行状态、模式、档位、温湿度、目标/反馈计数和PWM；不显示MQ-2，也不把未经标定的反馈计数标为RPM。
- 精简中文字体只包含界面实际字符；底部状态栏区分系统正常、DHT11失联、编码器故障和防回流功能预留。
- UI数据每500ms应用一次，LVGL处理检查周期为5ms；DHT11仍每2秒采集，超过6000ms显示失联并沿用现有AUTO安全停止策略。
- 验收分为软件构建、VM断开、VM接通空载和2小时稳定性四层；继续禁止扇叶、机械负载、堵转和带电插拔。
- 详细设计见`docs/superpowers/specs/2026-08-19-m9-smartvent-lvgl-ui-design.md`；用户复核书面设计后再生成逐步实施计划。

## 设计依据

完整设计见 `docs/superpowers/specs/2026-07-20-stm32f407-range-hood-controller-design.md`。开发板硬件依据为根目录 `stm32f407vet6.pdf`。

项目总路线见 `docs/superpowers/plans/2026-07-20-stm32f407-smart-hood-master-plan.md`。M3A设计见 `docs/superpowers/specs/2026-07-26-dht11-acquisition-design.md`，逐步实施计划见`docs/superpowers/plans/2026-07-26-m3a-dht11-acquisition.md`。
