# STM32F407 智能油烟机项目指南

## 当前状态

- 当前阶段：M1基础功能完成，准备进入下一里程碑；10分钟稳定性测试保留为未测试。
- 当前工程：`firmware/SmartHood`已创建，可由Keil MDK-ARM编译。
- 开发路线：分层增量开发。
- 第一阶段范围：主控功能，不包含Bootloader。

## 项目目标

基于 STM32F407VET6 和 FreeRTOS，实现传感器采集、单键交互、ST7735S显示、直流电机编码器测速、PID闭环、自动调速、手动档位和防回流迟滞控制。

## 已确认关键参数

- 电机额定转速：300 RPM。
- 电机型号：JGA12-N20-50B，6V额定，减速比50，空载40mA，堵转0.55A。
- 编码器：AB双相、7 PPR、开漏输出、3.3V供电；理论四倍频计数1400/输出轴圈。
- 手动目标转速：190 RPM和220 RPM。
- PID/测速周期：50 ms。
- 电机PWM目标频率：20 kHz。
- TFT：ST7735S，SPI接口。
- TFT实物：1.8英寸128×160，丝印顺序BLK/CS/DC/RST/SDA/SCL/VDD/GND。
- 用户输入：PA0单按键；RST仅复位。
- DHT11读取周期：1 s。
- MQ-2：第一阶段使用ADC值或相对值，不直接标注ppm。
- MQ-2模块：LM393 Flying-Fish模块，5V供电，AO必须分压后接STM32 ADC。
- 电源模块：3A DC-DC多路模块，输入6～30V，提供VIN直通、5V、3.3V和GND。
- 不使用开发板P2板载TFT插座；ST7735S通过杜邦线连接SPI2和独立GPIO。
- 板载W25QXX使用SPI1重映射引脚PB3/PB4/PB5，片选PA15。
- PA0板载按键按下为高电平，软件配置内部下拉并使用轮询。

## 里程碑进度

| 里程碑 | 内容 | 状态 |
|---|---|---|
| M1 | CubeMX、时钟、串口、FreeRTOS最小工程 | 功能通过：10分钟稳定性未测试 |
| M2 | ST7735S显示 | M1基线编译通过，正在配置CubeMX的SPI2与TFT控制GPIO |
| M3 | DHT11、MQ-2采集与滤波 | 未开始 |
| M4 | TB6612、电机开环PWM | 未开始 |
| M5 | 编码器测速与标定 | 未开始 |
| M6 | PID闭环 | 未开始 |
| M7 | 单键交互与模式管理 | 未开始 |
| M8 | 自动融合与防回流 | 未开始 |
| M9 | UI、异常处理、稳定性测试 | 未开始 |
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

## 设计依据

完整设计见 `docs/superpowers/specs/2026-07-20-stm32f407-range-hood-controller-design.md`。开发板硬件依据为根目录 `stm32f407vet6.pdf`。

项目总路线见 `docs/superpowers/plans/2026-07-20-stm32f407-smart-hood-master-plan.md`，当前里程碑计划见 `docs/superpowers/plans/2026-07-20-m1-cubemx-freertos-bringup.md`。
