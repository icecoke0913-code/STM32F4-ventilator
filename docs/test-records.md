# 测试记录

## 记录模板

### 测试编号：M?-T?

- 日期：
- 固件版本或提交：
- 测试目标：
- 接线条件：
- 电源电压：
- CubeMX关键配置：
- 操作步骤：
- 预期结果：
- 实际结果：
- 串口/TFT观测：
- 是否通过：
- 问题与原因：
- 解决方式：
- 后续动作：

## 计划验收项目

| 编号 | 验收内容 | 状态 |
|---|---|---|
| M1-T1 | 系统时钟、串口和FreeRTOS任务稳定运行 | 功能通过，10分钟稳定性未测试 |
| M2-T1 | ST7735S颜色、方向和刷新稳定 | 通过 |
| M3-T1 | DHT11连续读取、断线处理和自动恢复 | 通过 |
| M3-T2 | MQ-2 ADC范围、滤波和预热变化 | 未测试 |
| M4-T1 | 电机正转、停止和PWM调速 | 功能通过；助手接线照片核对由用户豁免 |
| M5-T1 | 编码器方向、计数和RPM趋势 | 功能通过：接线、计数回绕及三挡测速正常；实际CPR标定由用户跳过 |
| M6-T1 | 190 RPM稳态误差 | 未测试 |
| M6-T2 | 220 RPM稳态误差 | 未测试 |
| M6-T3 | 目标阶跃响应和堵转保护 | 未测试 |
| M7-T1 | 单键短按、双击和长按识别 | 通过：固定Tick自检及真实PA0交互；含回绕边界 |
| M8-T1 | 自动模式传感器响应 | 未测试 |
| M8-T2 | 防回流迟滞避免反复启停 | 未测试 |
| M9-T1 | 连续运行2小时稳定性 | 未测试 |

## M1环境记录

- STM32CubeMX：6.16.1
- STM32CubeF4：1.28.3
- 环境检查结果：通过
- MCU选择：STM32F407VETx、LQFP100，截图确认通过
- Project Name：SmartHood
- Project Location：`D:\Keil5 prj\stm32f4\firmware`
- Toolchain：MDK-ARM，Min Version V5.32
- Application Structure：Advanced
- Minimum Heap：0x400；Minimum Stack：0x800
- Firmware Package：STM32Cube FW_F4 V1.28.3
- Project Manager配置：截图确认通过
- SYS Debug：Serial Wire；HAL Timebase：TIM6
- RCC：HSE 8MHz，LSE Disable
- Main PLL：HSE / M=8 / N=336 / P=2
- 时钟结果：SYSCLK=168MHz，HCLK=168MHz，PCLK1=42MHz，APB1 Timer=84MHz，PCLK2=84MHz，APB2 Timer=168MHz
- 48MHz域：M1未启用相关外设，PLLQ由CubeMX锁定，忽略
- 时钟配置：截图确认通过
- PA1：GPIO推挽输出、初始High、No Pull、Low Speed、标签BOARD_LED
- PA0：GPIO输入、内部Pull-down、标签USER_KEY
- GPIO配置：用户确认完成
- USART1：Asynchronous，PA9 TX、PA10 RX
- 串口参数：115200、8 Bits、No Parity、1 Stop Bit、RX/TX、16 Samples
- Hardware Flow Control：Disable；DMA/USART1中断：M1不启用
- USART1配置：截图确认通过
- FreeRTOS Interface：CMSIS_V2
- FreeRTOS版本：10.3.1；CMSIS-RTOS版本：2.1.3
- TIM6/SysTick冲突：未出现
- FreeRTOS启用：截图确认通过
- configTOTAL_HEAP_SIZE：32768 Bytes
- defaultTask：osPriorityNormal、256 Words、StartDefaultTask、Dynamic、NULL
- 队列/互斥量/软件定时器：M1暂不创建
- FreeRTOS配置：用户确认完成
- Library Files：Copy only the necessary library files
- Generate peripheral initialization as pair of `.c/.h`：启用
- Keep User Code when re-generating：启用
- Delete previously generated files when not re-generated：保留启用
- Code Generator配置：截图确认通过
- IOC与MDK-ARM工程已生成，Keil能够打开`SmartHood.uvprojx`
- 首次编译阻塞：Keil缺少STM32F4设备支持包`STM32F4xx_DFP`
- CubeMX的STM32CubeF4 V1.28.3仅提供HAL/中间件源码，不能替代Keil Device Family Pack
- 当前Keil：MDK 5.24a（2017），PackUnzip 1.2.18
- 离线包：Keil.STM32F4xx_DFP.3.1.1.pack，Pack Schema 1.7.36（2025）
- `.Download`目录存在且Authenticated Users具有Modify权限，排除目录不存在和普通写权限原因
- 根因：旧PackUnzip不兼容DFP 3.1.1的新版licenseSets/多许可证元数据
- 用户选择临时兼容方案：已安装Keil.STM32F4xx_DFP 1.0.8
- 旧包安装位置：`D:\Keil5\Keil\STM32F4xx_DFP\1.0.8`
- 器件名差异：CubeMX工程为STM32F407VETx，旧DFP提供STM32F407VE
- Keil目标器件已映射为STM32F407VE，PackID=`Keil.STM32F4xx_DFP.1.0.8`
- 原始生成工程首次编译：0 Error(s)、0 Warning(s)
- Program Size：Code=11348、RO-data=496、RW-data=148、ZI-data=39364
- 输出文件：SmartHood.axf、SmartHood.hex、SmartHood.map
- Build Time：16秒
- 验证来源：`MDK-ARM/SmartHood/SmartHood.build_log.htm`
- App目录：`App/Inc`与`App/Src`已建立
- 串口日志模块：`debug_log.h/.c`已创建并加入Keil App分组
- Include Path：`..\App\Inc`
- 日志模块编译：0 Error(s)、0 Warning(s)，Build Time 2秒
- 当前Program Size未变化，因为`DebugLog_Printf`尚未被调用，链接器移除了未引用函数
- 下一检查点：创建app_tasks模块并实际调用DebugLog_Printf
- `app_tasks.h/.c`已创建，`app_tasks.c`已加入Keil的App分组
- 应用任务模块独立编译：0 Error(s)、0 Warning(s)，Build Time 2秒
- 此时Program Size仍为Code=11348、RO-data=496、RW-data=148、ZI-data=39364；原因是`App_DefaultTask`尚未接入FreeRTOS入口
- 下一检查点：在`freertos.c`的USER CODE区域包含`app_tasks.h`并委托默认任务
- `freertos.c`已在USER CODE Includes区域包含`app_tasks.h`
- 头文件接入编译：0 Error(s)、0 Warning(s)，Build Time 2秒
- `StartDefaultTask`已在USER CODE区域调用`App_DefaultTask(argument)`，原空循环已删除
- 应用任务集成编译：0 Error(s)、0 Warning(s)，Build Time 2秒
- 集成后Program Size：Code=18416、RO-data=1048、RW-data=148、ZI-data=39364
- M1软件构建检查通过，下一检查点为ST-Link下载与USART1硬件联调
- 首次打开ST-Link Debug Settings时提示`No ST-LINK detected`，Unit与Serial Number为空
- Windows已连接设备枚举中未发现ST-Link/CMSIS-DAP设备，故问题位于电脑到调试器的USB识别层，尚未进入STM32 SWD接线检测
- 更换并重新插入电脑USB口后，Keil已识别`ST-LINK/V2`，序列号`000000000001`，固件`V2J38S7`
- 当前提示变为`No target connected`，说明电脑到ST-Link连接已恢复，下一步检查开发板供电、共地及SWD接线
- 开发板通过USB-C供电且电源指示灯亮
- 将Keil连接方式由`under Reset`改为`Normal`后仍提示`No target connected`，因此并非仅由未连接NRST导致
- 下一步需要根据接口丝印逐端核对GND、SWDIO和SWCLK接线，并确认目标板3.3V电源轨
- 重新接线并连接ST-Link 3.3V与开发板SWD 3.3V后，Keil成功识别SWD：IDCODE=`0x2BA01477`，设备`ARM CoreSight SW-DP`，SWD时钟4MHz
- 因开发板同时由USB-C供电，当前存在两个3.3V电源可能并联的风险；烧录前需断电测试是否可移除ST-Link 3.3V线
- 保持USB-C供电但移除ST-Link 3.3V线后重新出现`No target connected`，确认该克隆ST-Link的SWD通信需要3.3V引脚参与
- 下一步采用单一供电源验证：断开USB-C，仅由ST-Link 3.3V给开发板供电，避免两个稳压输出直接并联
- 断开开发板USB-C、仅由ST-Link 3.3V供电后，开发板电源灯亮度降低但SWD仍能稳定识别目标
- M1烧录阶段暂定采用ST-Link单一3.3V供电，且不连接任何传感器、显示屏或电机模块
- ST-Link单独3.3V供电时随后出现`ST-LINK connection error`，固件显示异常为`V0J0S0`，Flash Download失败并提示`Target DLL has been cancelled`
- 结合开发板电源灯明显变暗，判定该ST-Link的3.3V输出不足以稳定给整块F407最小系统板供电；停止使用此供电方案
- 原理图确认开发板USB-C经5V电源轨和SPX1117M3-3.3稳压器生成板载3.3V，SWD口3V3与主3.3V电源轨相连
- 下一步需核实ST-Link与开发板两侧3.3V电压及当前SWD线序，不能继续盲目并联电源或重复烧录
- Windows设备枚举确认ST-Link当前使用`USB\\VID_0483&PID_3748`，设备名`STM32 STLink`，驱动状态Started；它本身并未被识别为U盘
- 资源管理器曾显示E:/F:可移动盘，但当前系统无实际E:/F:卷挂载；判断为开发板原厂USB Mass Storage固件产生的盘符或资源管理器缓存，与SWD连接失败无直接关系
- 后续下载日志明确提示`No Algorithm found for: 08000000H - 08004C9BH`与`Erase skipped!`
- 根因：Keil目标尚未配置内部Flash编程算法，并非应用代码编译错误
- 已安装DFP 1.0.8包含多种容量的Flash算法；最终选择需以该旧包对`STM32F407VE`的设备映射为准
- 进一步核对DFP 1.0.8的PDSC：`STM32F407VE`的IROM1仍定义为`0x80000`（512KB），但设备包默认下载算法明确映射为`STM32F4xx_1024.FLM`
- 因此旧Keil列表中应选择`STM32F4xx Flash`、Flash Size 1M；该算法是F407系列通用算法，工程链接范围仍限制为512KB
- Flash Download已添加`STM32F4xx Flash`通用算法，范围`0x08000000-0x080FFFFF`
- 下载选项已配置为`Erase Sectors`、`Program`、`Verify`、`Reset and Run`
- 烧录前仍需确认当前安全供电/三线SWD接法下目标IDCODE能够稳定识别
- SWD最终稳定识别：ST-Link固件`V2J38S7`，IDCODE=`0x2BA01477`，设备`ARM CoreSight SW-DP`，SWD时钟4MHz
- 固件已成功运行，USART1在115200、8N1下连续输出心跳
- 已观察串口样本：`heartbeat=92 key=0 tick=92188`至`heartbeat=100 key=0 tick=100204`
- 心跳序号连续，Tick每行约增加1000ms，未观察到重复启动；FreeRTOS、HAL时基和USART1基础运行通过
- 待验收：PA0按下/释放状态、PA1 LED翻转、累计10分钟无重启
- PA0按键测试：松开时`key=0`，按下时`key=1`，结果通过
- 10分钟连续运行测试：用户决定本轮跳过，状态记为未测试，不计入已通过项
- PA1 LED测试：按每秒一次翻转，结果通过
- M1-T1结论：基础功能通过；10分钟连续运行项按用户决定跳过，仍标记为未测试

### 2026-07-25：M2基线构建检查

- 当前分支：`feature/m2-st7735s`
- M2修改前的M1工程由用户在Keil中重新编译，结果无错误。
- 此结果仅确认软件基线可构建；ST7735S尚未接线、烧录或进行显示测试。

### 2026-07-25：M2 SPI2空外设构建

- CubeMX已生成SPI2、PB13/PB15复用配置以及PD4-PD7控制GPIO配置。
- Keil执行全量重新编译，`spi.c`和`stm32f4xx_hal_spi.c`均参与编译。
- 构建结果：0 Error(s)、0 Warning(s)，Build Time 17秒。
- Program Size：Code=18868、RO-data=1048、RW-data=148、ZI-data=39452。
- 本次仅验证生成代码和编译，尚未连接或测试ST7735S硬件。

### 2026-07-25：M2最小字模模块构建

- `BSP/Inc/fonts.h`和`BSP/Src/fonts.c`已创建，`fonts.c`已加入Keil BSP分组。
- 最新全量构建日志包含`compiling fonts.c...`。
- 构建结果：0 Error(s)、0 Warning(s)，Build Time 15秒。
- Program Size：Code=18866、RO-data=1050、RW-data=148、ZI-data=39452；当前字体接口尚未被应用层引用，最终链接尺寸变化有限。

### 2026-07-25：M2 ST7735S BSP首次构建

- 已实现阻塞式SPI2传输、硬件复位、背光控制和ST7735S初始化序列。
- 已实现地址窗口、RGB565连续像素写入、重复颜色填充、矩形、全屏、单像素和最小文本绘制。
- Keil BSP分组包含`fonts.c`和`bsp_st7735s.c`，最新日志同时显示两者参与编译。
- 构建结果：0 Error(s)、0 Warning(s)，Build Time 17秒。
- Program Size：Code=18866、RO-data=1050、RW-data=148、ZI-data=39452；应用层尚未引用驱动，链接器移除了未使用代码。
- 本检查点仍未连接或测试ST7735S硬件。

### 2026-07-26：M2显示自检集成构建

- `app_tasks.c`已引用ST7735S BSP，并在默认任务启动时执行一次显示测试。
- 最新日志包含`compiling app_tasks.c...`，构建结果为0 Error(s)、0 Warning(s)，Build Time 2秒。
- Program Size：Code=21234、RO-data=1238、RW-data=148、ZI-data=39452。
- 相比未引用驱动时Code和RO-data明显增加，确认BSP、字模及测试逻辑已进入最终固件。
- 尚未接线和烧录，本记录不代表显示硬件测试通过。

### 2026-07-26：M2首次上板白屏排查

- 固件烧录后USART1持续输出心跳，复位后输出`SmartHood M1 start`和`ST7735S init and test OK`，主任务未异常复位。
- 屏幕复位过程中先黑屏再白屏，之后始终白屏，没有出现红、绿、蓝、白、黑切换或最终测试画面。
- 先黑后白仅确认PD4背光控制生效；SPI为单向发送且屏幕没有MISO反馈，日志中的OK不能证明控制器收到命令。
- 当前M2-T1状态：失败/排查中。下一步核对RST、CS、DC、SCL、SDA物理连接，再以降低SPI时钟作为单变量测试。
- 重新插接显示通信线后屏幕恢复稳定显示，确认首次白屏根因为物理连接问题；无需降低SPI时钟。
- 初始`MADCTL=0xC8`时方向和坐标正确，但红蓝通道互换：左上红标显示为蓝、左下蓝标显示为红，青色与黄色也互换。
- 下一步仅清除MADCTL的BGR位，将竖屏值由`0xC8`改为`0xC0`后复测。
- `MADCTL`改为`0xC0`并重新烧录后颜色顺序正确：左上红、右上绿、左下蓝、右下黄；青色和黄色文字恢复正确。
- 四条边框和四个角均位于有效显示区域，最终坐标偏移保持X=0、Y=0。
- 固定在单像素白边上的彩色细纹为近距离照片分辨RGB子像素的效果，未观察到随机位置变化或闪烁，不按SPI杂点处理。
- 最新校准构建：0 Error(s)、0 Warning(s)，Code=21234、RO-data=1238、RW-data=148、ZI-data=39452。
- PA0按下为`key=1`、松开为`key=0`，PA1继续每秒翻转，USART1心跳持续递增。
- 约1分钟最终观察中显示稳定，无随机闪烁、异常发热或重复启动信息。
- M2-T1结论：通过。最终值为MADCTL=0xC0、X Offset=0、Y Offset=0、背光高电平有效、SPI2=5.25MHz。

### 2026-07-26：M3-T1 DHT11验收计划

- 当前状态：设计已确认，尚未配置、编译或接线。
- 读取周期：2秒；数据通过USART1输出，TFT保持M2测试画面。
- 未连接启动：每约2秒报告TIMEOUT，默认任务心跳、PA0、PA1和TFT继续正常运行。
- 正常接线：连续取得至少10次有效温湿度读数。
- 变化测试：缓慢哈气后湿度值出现可观察变化，避免水汽凝结。
- 断线测试：运行中断开DATA后报告TIMEOUT，系统不阻塞或复位。
- 恢复测试：重新连接DATA后不按RST，在后续读取周期自动恢复OK。
- 串口回归：心跳与DHT11日志无乱码、无明显交叉或丢失。
- MQ-2保持未连接；M3-T2等待万用表、分压电阻和面包板。

### 2026-07-26：M3A基线与CubeMX空外设构建

- 功能分支：`codex/feature-m3a-dht11`。
- M2基线Rebuild：Code=21234、RO-data=1238、RW-data=148、ZI-data=39452，0 Error(s)、0 Warning(s)，Build Time 14秒。
- CubeMX配置：PD0=`DHT11_DATA`、Input Pull-up；TIM5内部时钟、PSC=83、ARR=0xFFFFFFFF、Up、DIV1、无中断。
- sensorTask：osPriorityNormal、256 Words、Dynamic、入口`StartSensorTask`；当前为未接入业务的空循环。
- 生成保护点：`App_DefaultTask(argument)`保留；Keil使用DFP 1.0.8和已验证的STM32F4xx Flash下载算法。
- 空外设Rebuild日志包含`compiling tim.c...`。
- 空外设构建结果：Code=21886、RO-data=1286、RW-data=152、ZI-data=39528，0 Error(s)、0 Warning(s)，Build Time 16秒。
- 本检查点尚未修改DebugLog、创建DHT11 BSP、烧录新固件或连接DHT11。

### 2026-07-27：M3A DebugLog互斥保护回归

- 修改内容：新增`DebugLog_Init()`，创建CMSIS-RTOS2互斥量；`DebugLog_Printf()`在格式化和USART1发送期间获取并释放互斥量。
- 初始化位置：`MX_FREERTOS_Init()`的USER CODE Init区域，位于defaultTask和sensorTask创建之前。
- 构建结果：Code=23150、RO-data=1286、RW-data=156、ZI-data=39524，0 Error(s)、0 Warning(s)，Build Time 2秒。
- 烧录条件：DHT11、MQ-2、TB6612和电机均未连接，保留已验证的TFT、ST-Link与USART1调试连接。
- 串口结果：启动信息和心跳正常，PA0松开/按下继续显示key=0/1，无乱码、半行交叉或重复启动。
- 硬件回归：PA1继续每秒翻转，ST7735S保持M2测试画面，结果通过。
- sensorTask仍为CubeMX空循环，本记录不代表DHT11采集功能通过。

### 2026-08-02：M3A DHT11 BSP独立构建

- 新增文件：`BSP/Inc/bsp_dht11.h`、`BSP/Src/bsp_dht11.c`。
- Keil BSP分组已包含`bsp_dht11.c`，全量构建日志包含`compiling bsp_dht11.c...`。
- 驱动检查：所有GPIO边沿等待均带120 μs超时；18 ms启动阶段不关中断；30 μs释放等待、响应和40位采集期间保存并屏蔽中断；返回前恢复中断和PD0输入模式。
- 数据检查：接收5字节并验证校验和；只有OK状态才更新温湿度输出结构。
- 构建结果：Code=23150、RO-data=1286、RW-data=156、ZI-data=39524，0 Error(s)、0 Warning(s)，Build Time 18秒。
- 程序大小未增加的原因是应用层尚未调用DHT11接口，未引用驱动代码可被链接器移除。
- 本检查点仅代表驱动独立编译通过，尚未进行TIMEOUT或真实传感器测试。

### 2026-08-02：M3A SensorTask集成构建

- `StartSensorTask()`已委托`App_SensorTask()`，不再运行CubeMX默认1 ms空循环。
- SensorTask行为：启动TIM5、等待2秒、读取DHT11、输出状态、延迟2秒后重试。
- 首次构建结果为0 Error(s)、1 Warning(s)；唯一警告为`app_tasks.c`文件末行缺少换行符，不涉及程序逻辑。
- 修正文件末尾换行后重新全量构建，日志包含`compiling bsp_dht11.c...`、`compiling app_tasks.c...`和`compiling freertos.c...`。
- 最终结果：Code=24002、RO-data=1286、RW-data=156、ZI-data=39524，0 Error(s)、0 Warning(s)，Build Time 17秒。
- 程序大小相对BSP未引用阶段增加，确认DHT11驱动与SensorTask日志逻辑进入最终镜像。
- 本记录仅确认软件集成构建，尚未烧录或执行未连接TIMEOUT测试。

### 2026-08-03：中文注释零行为变化验收与M3A未连接测试

- 注释范围：`App/Inc`、`App/Src`、`BSP/Inc`、`BSP/Src`全部自编模块，以及`Core/Src/freertos.c`的USER CODE区域。
- 文件编码：先以`debug_log.h`进行UTF-8小文件试验，用户确认Keil中文显示正常；随后完成其余模块的结构化中文注释。
- 注释内容覆盖模块职责、公开接口、FreeRTOS任务流程、DebugLog并发保护、DHT11微秒时序与恢复、ST7735S初始化/坐标/RGB565绘图及5×7字模回退逻辑。
- 静态审计：将注释和空白剥离后，与注释前提交`b76f8ec`逐文件比较，11个目标文件的有效代码全部一致；`freertos.c`差异全部位于USER CODE区域。
- 最终Rebuild日志包含`fonts.c`、`debug_log.c`、`bsp_dht11.c`、`bsp_st7735s.c`、`app_tasks.c`和`freertos.c`，说明相关模块均重新编译。
- 最终构建：Code=24002、RO-data=1286、RW-data=156、ZI-data=39524，0 Error(s)、0 Warning(s)。
- 最终HEX SHA-256：`0DC8FC5B0E3CDCCAF4860CBD36F1DF81B97263A7F2E895ED10C4A2DDCA7C96E8`，与注释前基线完全一致。
- 用户上板确认：启动日志与心跳持续正常；DHT11未连接时约每2秒输出`DHT11 status=TIMEOUT`；PA0松开/按下为`key=0/1`；PA1每秒翻转；ST7735S测试画面正常。
- 验收结论：中文注释未改变固件行为，M1/M2功能无回归，M3A未连接TIMEOUT与自动周期重试测试通过。

### 2026-08-03：M3A DHT11正常采集与湿度变化测试

- 接线：三针DHT11模块使用3.3V供电，DATA接PD0，GND与开发板共地；MQ-2、TB6612和电机保持不连接。
- 串口参数：COM6、115200波特率、8位数据、1位停止、无校验；日志文本显示正常且没有任务间交叉。
- 连续采集：多次输出`status=OK`，截图样本温度为30.8～31.3°C，常态湿度约70.0%～71.0%。
- 湿度变化：缓慢哈气后湿度上升到87.0%，随后回落到70.0%，说明传感器能够响应环境湿度变化。
- 回归观察：截图中heartbeat从62持续递增到70，PA0保持`key=0`，未出现系统阻塞、复位或串口乱码。
- 当前结论：DHT11供电、PD0单总线通信、数据校验和定点数日志输出正常；下一步执行两次DATA运行中断线与无复位自动恢复测试。

### 2026-08-03：M3A DHT11断线恢复与最终验收

- 测试方法：系统持续运行时只断开DHT11模块一侧DATA线，3.3V和GND保持不动，不按RST且不重新烧录。
- 第一次断线：后续采样由`status=OK`切换为`DHT11 status=TIMEOUT`，heartbeat、PA0、PA1和TFT继续工作。
- 第一次恢复：重新接入DATA后，在后续读取周期自动恢复`status=OK`，没有复位或人工干预。
- 第二次重复：再次执行DATA断开和接回，TIMEOUT与自动恢复OK行为可以重复。
- 完整配置：PD0输入上拉；TIM5 PSC=83、ARR=0xFFFFFFFF、1 MHz自由运行且无中断；SensorTask为Normal优先级、256 Words动态栈、约2秒采样周期。
- 最终固件：Code=24002、RO-data=1286、RW-data=156、ZI-data=39524，0 Error(s)、0 Warning(s)，HEX SHA-256为`0DC8FC5B0E3CDCCAF4860CBD36F1DF81B97263A7F2E895ED10C4A2DDCA7C96E8`。
- 最终结论：正常采集、湿度变化、未连接TIMEOUT、运行中断线、两次无复位自动恢复及M1/M2回归全部通过，M3A验收完成。

### 2026-08-06：M4无电机软件安全测试

- 测试条件：TB6612、DC-DC、9V适配器、电机和编码器全部未连接；保留STM32、ST-Link、USB转TTL、ST7735S和DHT11。
- 固件构建：Code=25710、RO-data=1290、RW-data=160、ZI-data=39600，0 Error(s)、0 Warning(s)。
- 启动日志显示`motor init ok, state=STOP`；该结果只表示TIM4_CH1 PWM成功启动，不代表驱动模块已连接。
- PA0短按挡位日志按`30% → 50% → 70% → 0%停止 → 30%`循环，每次短按只切换一个挡位。
- PA0持续按住至少2秒不会连续切挡；松开不切挡，再次短按才进入下一挡，20ms轮询和40ms消抖行为通过。
- 软件处于70%挡位时按RST，重启后恢复`state=STOP`，不会自动恢复原挡位；再次短按从30%开始。
- 截图样本确认50%和70%日志、`key=1`到`key=0`变化、heartbeat持续递增以及DHT11 `status=OK`并行输出正常。
- PA1继续约每秒翻转，ST7735S画面正常，无重复启动、乱码或任务卡死。
- 结论：M4无电机软件安全测试通过；默认应在完全断电状态接线并由助手核对照片后再接通9V。后续用户明确授权跳过照片核对，因此该项未执行，由用户自行完成逐点接线确认。

### 2026-08-07：M4 TB6612空载电机硬件验收

- 接线：PB6/TIM4_CH1接PWMA，PB7接AIN1，PB8接AIN2，PB9接STBY；STM32 3.3V接VCC，DC-DC 5V接VM，STM32、TB6612与DC-DC共地。
- 电机实际线序：黑色M1接AO1，绿色M2接AO2；橙色VCC、黄色C2、白色C1和红色GND四根编码器线保持悬空并分别绝缘。
- 用户明确选择跳过助手接线照片核对，依据自行逐点检查结果继续上电；当前没有万用表，未实测DC-DC 5V和电机启动压降。
- 分级上电：先给STM32上电时日志为`motor init ok, state=STOP`；随后在停止状态接通9V，电机保持不转，无异味、冒烟、异常声音、迅速发热或系统复位。
- 30%挡位能够正常自行起转，运行稳定；按RST后电机停止，重启保持`state=STOP`且不恢复原挡位。
- 30%、50%、70%空载速度呈逐级上升趋势，方向始终一致；从70%切到0%后电机正常逐渐停止。
- 带电机长按PA0至少2秒不会连续跳挡，松开不切挡，再次短按才进入下一挡。
- 当前旋转方向符合最终安装需求，因此未交换AO1/AO2，也未增加软件反转。
- heartbeat、PA1、DHT11、ST7735S和USART1日志回归正常，无乱码、任务卡死或异常复位。
- 测试严格保持空载和短时运行；未安装扇叶，未进行堵转、带机械负载或长时间温升测试。
- 最终Rebuild：Code=25710、RO-data=1290、RW-data=160、ZI-data=39600，0 Error(s)、0 Warning(s)，Build Time 15秒。
- 构建日志确认`bsp_motor.c`、`app_tasks.c`和`tim.c`参与编译与最终链接。
- 最终HEX SHA-256：`950DDFECC1181A4EEA73A27C3AE9B8E1BA66B6B03680289E0A372C029BD991BF`。
- 最终结论：M4开环电机软件、空载硬件、最终构建及所有已执行的一致性检查通过；助手接线照片核对由用户明确豁免、未执行。下一步进入M5编码器设计。

### 2026-08-15：M5编码器接入与空载测速

- 编码器使用3.3V供电；白色C1接PC6/TIM3_CH1，黄色C2接PC7/TIM3_CH2，红色GND与STM32共地；C1/C2实际输出类型仍未使用万用表确认。
- TIM3使用Encoder Mode TI1 and TI2，PSC=0、ARR=65535，两路Rising Edge、Direct TI、DIV1、Filter=4，PC6/PC7启用内部上拉，不使用中断或DMA。
- 未接编码器烧录测试稳定输出`count=0 delta=0 dir=stopped rpm=0.0`；heartbeat持续递增，DHT11保持`status=OK`，多任务日志无交叉或乱码。
- 手动转动减速箱输出轴不能可靠反拖50:1齿轮箱，编码器无变化；轻转编码器侧转子时RPM发生变化，说明3.3V供电、C1/C2输入和TIM3计数链路有效。未继续用力反拖输出轴。
- 固定M4正转在当前C1/C2相序下计数方向为`reverse`。截图样本在50%挡位约为`delta=-1311`至`-1320`/500 ms、`rpm=-112.2`至`-113.0`。
- 三挡空载结果：30%约`-61 RPM`，50%约`-113 RPM`，70%约`-167 RPM`；转速随PWM占空比单调上升。
- 反向递减计数跨越0时从1222回绕到65411，日志仍保持约`-112 RPM`，16位回绕差值处理通过。
- 回到0%后日志恢复`delta=0 dir=stopped rpm=0.0`；测试期间无异常气味、明显发热、异常摩擦声或系统重启。
- 当前RPM使用理论`1400 counts/输出轴圈`换算；10圈约16秒的粗测与显示值存在明显偏差，但用户明确决定不继续实际CPR标定，因此该项记录为跳过、未执行，不把RPM作为精确转速。M5保留固定正转对应`reverse`的方向关系，未执行人为双向精确验证。
- 合并到`main`后的最终Rebuild：Code=26986、RO-data=1338、RW-data=164、ZI-data=39668，0 Error(s)、0 Warning(s)，Build Time 23秒。
- 最终HEX SHA-256：`7E9543FC3A23014FC07FB54B5D550A6986F867067CF8B40C7C0B55F1B8F612E4`。

### 2026-08-15：M6 PI控制器红灯、绿灯与板端自检

- 新增纯算法`Control`模块，公共接口和实现不访问HAL、FreeRTOS或电机驱动，使用Q8定点比例和积分系数。
- 自检覆盖零误差前馈、正误差增速、负误差减速、30%/90%输出限幅和积分抗饱和。
- TDD红灯构建成功编译`control_pi_selftest.c`，链接阶段仅报告`ControlPi_Init`、`ControlPi_Update`、`ControlPi_Reset`和`ControlPi_GetIntegral`未定义，结果为4 Error(s)、0 Warning(s)。
- 加入最小PI实现后的绿灯Rebuild：Code=27446、RO-data=1338、RW-data=164、ZI-data=39668，0 Error(s)、0 Warning(s)。
- 隔离电机输出执行板端自检，串口输出`control PI self-test PASSED`和`encoder start ok`；同时`motor init ok, state=STOP`、DHT11 `status=OK`、ST7735S启动测试与heartbeat均正常。
- 初次验证完成后将`APP_CONTROL_PI_SELF_TEST_ENABLED`恢复为`0U`；`control_pi.c`和`control_pi_selftest.c`仍参与编译，但未被运行时路径引用的代码由链接器移除。
- 关闭临时自检后的最终Rebuild：Code=26986、RO-data=1338、RW-data=164、ZI-data=39668，0 Error(s)、0 Warning(s)，Build Time 14秒。
- 程序已下载并通过Flash Verify；最终HEX SHA-256为`79828DBFA4406B3F92A9A44793F90D38EF316B634C86E95CAC9A20AFD7B914C5`。
- 本检查点只验证PI纯算法和既有功能回归，不代表电机闭环、软启动或编码器故障锁存已经实现或通过。

### 2026-08-15：M6电机命令所有权迁移构建

- 新增长度为4的CMSIS-RTOS2消息队列，PA0每次有效按下只发送一个`APP_MOTOR_COMMAND_NEXT`命令。
- 队列在`MX_FREERTOS_Init()`中、日志互斥量初始化成功后且所有任务创建前建立；创建失败进入`Error_Handler()`。
- `App_DefaultTask()`已删除TIM4启动、TB6612初始化、挡位表和直接调用`BSP_Motor_SetDuty()`/`BSP_Motor_Stop()`的路径。
- 静态检查确认DefaultTask不存在`BSP_Motor_*`引用；后续只有`App_MotorTask()`可以拥有电机状态并修改PWM。
- Rebuild结果：Code=26622、RO-data=1334、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 16秒。
- 此检查点的MotorTask仍保持M5编码器测速实现，尚未消费NEXT命令；因此未烧录，也未进行PA0或电机硬件运行测试。

### 2026-08-15：M6闭环状态机集成构建

- MotorTask成为唯一允许初始化TB6612、修改PWM和改变电机状态的任务；DefaultTask继续只发送NEXT命令。
- 状态机包含`STOP`、`LOW_START`、`LOW_PI`、`HIGH_START`、`HIGH_PI`和锁存`FAULT`，PA0按STOP、低档、高档、STOP顺序切换。
- 低档目标为130 counts/50ms、前馈50%；高档目标为195 counts/50ms、前馈70%；PI初始参数为Kp=64/256、Ki=4/256。
- 两个挡位均先以30% PWM软启动300ms，再进入PI；控制输出限制为30%至90%。
- PI状态连续10个50ms周期没有超过1 count的有效反馈时调用`BSP_Motor_Stop()`，归零PWM、拉低STBY并锁存`ENCODER_TIMEOUT`故障。
- FAULT状态第一次PA0只清除故障并保持STOP，第二次PA0才允许重新进入低档软启动；不实现自动重试。
- 每约500ms输出状态、目标计数、平均实际计数、误差、PWM、积分、故障和方向日志，不再把相对计数描述为精确RPM。
- Rebuild结果：Code=28042、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 16秒。
- 本检查点只完成软件集成构建，尚未烧录或执行无电机故障锁存、低档闭环、高档阶跃和编码器断线测试。

### 2026-08-16：M6无电机故障锁存验证

- 测试前完全断电并断开TB6612的VM，STM32、ST-Link、USB转TTL和逻辑接线保持连接；未在通电状态插拔电机或编码器线。
- 烧录M6闭环固件后，上电持续输出`state=STOP`、`target=0`、`actual=0`、`duty=0`、`integral=0`和`fault=0`，电机控制默认保持停止。
- STOP状态短按PA0后进入低档软启动；因VM断开、编码器无有效计数，约在规定窗口后输出`motor state=FAULT reason=ENCODER_TIMEOUT duty=0%`。
- FAULT锁存期间持续输出`state=FAULT`、`target=0`、`actual=0`、`duty=0`、`integral=0`和`fault=1`，没有自动重试或恢复输出。
- 清除后第二次按键可以重新进入LOW；截图样本在无反馈时PI输出上升到86%，未超过90%上限，随后再次触发ENCODER_TIMEOUT并归零。
- FAULT状态只短按一次PA0后，日志连续多次保持`state=STOP`、`duty=0`和`fault=0`，heartbeat从37增长到39，确认清故障不会自动重新启动。
- DHT11在测试期间继续输出`status=OK`，heartbeat持续运行；未观察到任务卡死或异常复位。
- 结论：上电STOP、无反馈故障停机、FAULT持续锁存、人工清除保持STOP和第二次按键才重启均通过；本记录不代表带电机闭环或负载测试通过。

### 2026-08-16：M6低档空载闭环验收

- 在完全断电状态恢复TB6612 VM，保持原M4/M5电机和编码器接线，不安装扇叶或机械负载。
- 先给STM32上电确认STOP，再在STOP状态接通9V电源；观察3秒电机没有自行转动，无异常气味、明显发热或系统复位。
- PA0短按后经过30% PWM、300ms软启动进入LOW，连续空载运行约10秒无持续振荡、异常声音、明显发热或复位。
- 保持初始参数Kp=64/256、Ki=4/256，低档目标为130 counts/50ms；截图样本actual为129至130、duty为48%至49%、integral约为-119至-122、fault=0、方向为reverse。
- 实际计数位于117至143验收范围内，PWM明显低于90%上限，因此未执行参数调整。
- LOW状态经PA0进入高档后再次按PA0，电机正常进入STOP；重新进入LOW后按RST，电机立即停止且重启保持STOP。
- 结论：低档相对计数闭环、PA0停止和RST停止均通过；结果仅适用于当前5V VM、空载和当前电机接线，不代表精确RPM或带负载性能。

### 2026-08-16：M6高档空载与跳过项记录

- 低档稳定后通过PA0切换到高档，用户连续观察后确认运行无问题，未报告持续振荡、FAULT、异常声音、异味、明显发热或系统复位。
- 本次没有提供高档`actual`和`duty`数值截图，因此不记录精确范围，也不据此声称195 counts/50ms目标的数值验收已完成。
- 高档到STOP的PA0停止路径已在低档验收阶段执行并由用户确认正常。
- 用户明确选择跳过断电后断开C1/C2的编码器断线测试，因此未执行该项，也不把未执行结果描述为通过。
- 因未断开C1/C2，无需执行恢复编码器接线步骤；现有电机和编码器接线保持不变。
- 当前M6已验证PI纯算法、上电STOP、VM隔离无反馈FAULT、故障清除、低档空载闭环、高档空载主观运行、PA0停止和RST停止；编码器实际断线、带负载、堵转和长时间温升仍未测试。
- M6最终Rebuild包含`control_pi.c`、`control_pi_selftest.c`和`app_tasks.c`，结果为Code=28042、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 17秒。
- 最终HEX SHA-256为`F85F226CE1B5F8F3FEBE85360801217742E2ED39B3CA8E6A53313A6F0016CA55`。
- 范围检查确认未实现自动模式、MQ-2融合、软件反转、LVGL或未经测试的堵转保护；这些功能继续留在后续阶段。

### 2026-08-16：M7按键状态机TDD红灯

- 新增`bsp_key.h`，定义40ms消抖、350ms双击窗口、1000ms长按阈值、按键上下文和`NONE/SHORT/DOUBLE/LONG`事件。
- 新增`bsp_key_selftest.h/.c`，使用固定电平和毫秒Tick覆盖抖动、单击延迟确认、双击仲裁、长按一次性、上电按住保护和32位Tick回绕。
- Keil新增`BSP Test` Group和`..\BSP\Test`包含路径；临时打开`APP_M7_SELF_TEST_ENABLED`并在MotorTask初始化电机前调用自检。
- 未创建`bsp_key.c`时执行Rebuild，链接阶段只报告`BSP_Key_Init`、`BSP_Key_IsPressed`和`BSP_Key_Process`三个未定义符号。
- 构建结果为3 Errors、3 Warnings、Target not created；Warnings均为链接失败后无法列出镜像符号或加载地址，不是额外源码问题。
- 结论：红灯失败原因与缺失的按键生产实现完全一致，自检已证明会约束下一步实现；当前固件不可烧录。

### 2026-08-16：M7按键状态机绿灯

- 新增`bsp_key.c`，实现无符号Tick时间差、上电按住保护、40ms消抖、350ms单双击仲裁和1000ms一次性长按。
- `bsp_key.c`与`bsp_key_selftest.c`均参与完整Rebuild，结果为Code=29010、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 14秒。
- VM断开时烧录自检固件，串口显示`M7 key self-test PASSED`，随后输出`motor control ready, state=STOP`并持续保持控制状态STOP。
- 截图中串口开始接收位置在`M7`文字前出现少量一次性乱码；后续电机、控制和DHT11日志均完整清晰，当前不记录为持续串口乱码故障。
- DHT11继续输出`status=OK`，样本为24.8摄氏度、54.0%RH；电机保持停止，M6控制任务未因自检阻塞。
- 结论：按键状态机确定性自检在板端通过，既有MotorTask和DHT11功能无阻塞回归；本检查点尚未把真实PA0输入切换到新按键模块。

### 2026-08-16：M7模式管理器TDD红灯

- 新增`mode_manager.h`，定义运行许可、AUTO/MANUAL/BACKFLOW模式、手动LOW/HIGH预选、编码器超时故障、事件处理结果和电机请求接口。
- 新增`mode_manager_selftest.h/.c`，验证上电安全初始状态、待机模式切换、MANUAL双击换挡、长按取得运行许可、BACKFLOW停机、非MANUAL双击忽略、故障优先停机、故障中事件屏蔽及长按清故障复位。
- `mode_manager_selftest.c`已加入Keil的`Control Test` Group；MotorTask的临时M7自检入口会依次执行按键识别自检和模式转换自检。
- 未创建`mode_manager.c`时执行完整Rebuild，全部源文件编译成功，链接阶段只报告`ModeManager_Init`、`ModeManager_HandleEvent`、`ModeManager_SetFault`和`ModeManager_GetMotorRequest`四个未定义符号。
- 构建结果为4 Errors、0 Warnings、Target not created，Build Time 16秒；UV4退出码为2，与预期红灯一致。
- 结论：模式转换表自检能够约束尚未实现的四个生产接口；当前固件不可烧录，下一步才创建最小`mode_manager.c`取得绿灯。

### 2026-08-16：M7模式管理器构建绿灯

- 新增`mode_manager.c`，实现安全初始状态、故障优先事件屏蔽和长按清除、运行许可切换、三模式短按循环、MANUAL双击换挡及电机请求映射。
- `mode_manager.c`已加入Keil的`Control` Group，并与`mode_manager_selftest.c`、`bsp_key_selftest.c`共同参与完整Rebuild。
- 模式管理自检进一步覆盖NULL安全、NONE事件状态不变、MANUAL高低挡往返、RUNNING返回STANDBY、BACKFLOW返回AUTO、RUNNING加AUTO保持STOP，以及AUTO和BACKFLOW拒绝双击。
- 新增独立的STANDBY/MANUAL、STANDBY/BACKFLOW和RUNNING/AUTO短按转换组合，并验证API清故障完整安全复位、非法运行状态/模式/挡位安全停机，以及故障中SHORT和DOUBLE不改变完整状态快照；原有故障锁存和长按清除检查继续保留。
- 扩展自检后的最终Rebuild结果为Code=30178、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 15秒；UV4退出码为0。
- 在TB6612 VM保持断开时烧录并按RST重新启动，串口明确输出`M7 self-test PASSED`和`motor control ready, state=STOP`。
- 自检通过后控制日志持续显示`state=STOP`、`target=0`、`actual=0`、`error=0`、`duty=0`、`integral=0`、`fault=0`和`dir=stopped`，未出现电机输出请求或故障锁存。
- 截图中`SmartHood M1 start`晚于MotorTask启动日志出现，属于FreeRTOS任务并发调度造成的日志先后顺序，不影响自检和安全停止结论。
- 结论：模式管理纯状态机的扩展自检已在板端通过，既有MotorTask在VM隔离条件下保持STOP；Task 5完成。当前真实PA0事件仍未迁移到新队列，留给Task 6。

### 2026-08-16：M7按键事件队列迁移构建

- `app_tasks.c`已直接包含`bsp_key.h`；长度为4的消息队列元素改为`BSP_KeyEvent_t`，NONE事件和未创建队列均拒绝发送，其余事件使用0超时入队。
- DefaultTask删除原有40ms手写消抖状态，显示自检后读取PA0初始电平并初始化`BSP_Key_t`，之后每20ms调用`BSP_Key_Process()`；有效事件入队失败时输出`key event queue full`。
- 心跳日志通过`BSP_Key_IsPressed()`读取稳定按键状态；静态检查确认旧命令、旧发送函数和旧候选/稳定状态符号均无匹配，DefaultTask内没有`BSP_Motor_*`调用。
- MotorTask的队列接收局部变量同步为`BSP_KeyEvent_t`，本检查点暂时让每个有效事件沿用M6单步状态切换；尚未接入ModeManager，也未执行Task 7的旧状态转换删除。
- 最终完整Keil Rebuild结果：Code=30182、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 26秒，UV4退出码为0。
- 本检查点未烧录、未操作硬件、未执行硬件运行测试；Task 6完成。该中间固件不得烧录，下一步必须由Task 7接入ModeManager并删除旧M6单步切换。

### 2026-08-17：M7模式请求与电机状态机软件集成

- `app_tasks.c`直接包含`mode_manager.h`；MotorTask在PI和电机初始化前建立ModeManager安全初值，并输出`STANDBY AUTO LOW NONE`初始模式。
- MotorTask每50ms清空已有按键事件队列，每个事件均交给`ModeManager_HandleEvent()`并记录运行许可、模式、挡位、故障和处理结果；非法枚举统一记录为`INVALID`，不误报有效状态。
- 新增app内纯映射自检`App_MotorModeIntegration_RunSelfTests()`，覆盖STOP、LOW、HIGH、FAULT及非法请求的内部状态、30%或0%初始占空比和立即停机标志，并接入`APP_M7_SELF_TEST_ENABLED`组合自检。
- TDD RED阶段只声明并调用`App_MotorRequestToAction()`而不实现；完整Rebuild在链接阶段仅报告`L6218E: Undefined symbol App_MotorRequestToAction (referred from app_tasks.o)`，结果为1 Error(s)、0 Warning(s)、Target not created，Build Time 21秒。
- GREEN阶段实现单一的安全纯映射，非法请求退化为STOP、0%和立即停机；删除M6旧的顺序切换分支，生产请求变化路径复用该action进入LOW/HIGH的30%软启动，或立即进入STOP/FAULT，不复制第二套请求映射。
- 请求变化或编码器故障锁存时设置`skip_log_sample`；该周期仍完整执行控制和故障处理，但其编码器delta属于前一50ms状态，不参与平均日志统计。周期末统一清空`actual_sum`、`signed_delta_sum`和`log_sample_count`，新的约500ms窗口从下一完整新状态或FAULT周期开始，不再混入切换周期样本。
- 编码器连续无反馈触发时先向ModeManager锁存`ENCODER_TIMEOUT`并同步FAULT请求，再保留原有故障停机、PI复位和日志，同时清零无反馈内部计数；故障触发周期的目标计数立即归零，长按清故障后请求保持STOP，不自动启动。
- 静态检查确认`NEXT`、`APP_MOTOR_COMMAND`和`App_MotorPostNext`在`firmware/SmartHood/App`中均无匹配；原有300ms软启动、PI控制和约500ms平均日志保持。
- 最终GREEN完整Keil Rebuild包含`app_tasks.c`、`mode_manager.c`、`mode_manager_selftest.c`和`bsp_key_selftest.c`，结果为Code=30646、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 18秒。
- 本检查点仅完成软件集成和构建验证；既有ModeManager自检未修改，新增app内纯映射自检尚未烧录或执行板端运行；本次未操作硬件或执行硬件验收。Task 7 Step 1至Step 6完成，Step 7提交留给主代理。

### 2026-08-17：M7无VM按键与故障交互验收

- 测试期间TB6612 VM保持断开；烧录`APP_M7_SELF_TEST_ENABLED=1U`版本后MotorTask继续启动并输出模式日志，证明按键、ModeManager和app内纯映射三项组合自检均通过。
- 上电保持`STANDBY + AUTO + LOW + NONE`和电机STOP；短按依次得到`MANUAL`、`BACKFLOW`、`AUTO`，每次只改变一个模式，控制日志持续为`state=STOP`、`duty=0`和`fault=0`。
- 在STANDBY的MANUAL模式下，两次双击分别完成`LOW -> HIGH`和`HIGH -> LOW`，模式不变且未产生电机启动请求。
- 长按约1秒只产生一次`RUNNING + MANUAL + LOW`事件，释放后未出现额外SHORT；无反馈时LOW控制输出达到90%软件上限，随后锁存`ENCODER_TIMEOUT`并立即变为`state=FAULT`、`target=0`、`duty=0`、`integral=0`和`fault=1`。
- 故障中的短按和双击各产生一次`IGNORED_FAULT`，运行许可、模式、挡位和故障均保持不变；长按产生`FAULT_CLEARED`并完整恢复`STANDBY + AUTO + LOW + NONE`，同周期控制回到STOP且没有自动重启。
- 整个测试中heartbeat持续递增，DHT11持续输出`status=OK`，样本为26.7摄氏度、69.0%RH；串口日志未出现持续乱码或任务阻塞。
- 无VM板端交互验收完成后，将`APP_M7_SELF_TEST_ENABLED`恢复为`0U`；自检代码继续保留在工程中，但正式启动不再执行临时确定性自检。
- 首次关闭开关后的Rebuild因app内静态自检函数仍参与编译而产生1个`#177-D declared but never referenced`警告；根因是调用被条件编译移除而函数定义未使用相同开关，未忽略该警告。
- 将`App_MotorModeIntegration_RunSelfTests()`函数定义纳入同一`APP_M7_SELF_TEST_ENABLED`条件编译后再次完整Rebuild，结果为Code=28778、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 16秒；生产请求映射函数仍由MotorTask正常使用。
- 关闭临时自检后的正式HEX SHA-256为`EB81C1253245E82124DA8521D648EFC3183B8CEFEA7628E532ACD5AAB96AC8A4`。
- 结论：Task 8的VM隔离按键、模式、故障交互、系统回归和正式0警告构建全部通过；本检查点不代表接通VM后的真实电机运行验收。

### 2026-08-17：M7接通VM空载验收与最终构建

- 未安装扇叶或机械负载；在完全断电状态恢复M6已验证的TB6612 VM、电机和编码器接线，先给STM32上电确认`STANDBY + AUTO + LOW`和STOP，再接通原9V电机电源。
- 接通9V后等待3秒且不操作PA0，电机没有自行转动，未报告异味、明显发热或系统复位，上电安全停止通过。
- 长按进入RUNNING AUTO时电机继续停止；短按进入MANUAL LOW后先执行30%软启动，再进入LOW PI。截图样本`actual=127~134`、`duty=49~52%`、`fault=0`，空载运行正常。
- MANUAL模式双击进入HIGH并重新执行30%软启动；用户确认高档转速高于低档、运行正常且未进入FAULT，但本次没有提供高档`actual`和`duty`数值，因此不记录精确高档闭环范围。
- MANUAL短按进入BACKFLOW后电机正常停止且未重新启动；低档运行中长按进入`STANDBY + MANUAL + LOW`后PWM立即归零，编码器惯性余量从`actual=22`回到0；高档运行时RST也立即停止并恢复`STANDBY + AUTO + LOW`。
- 系统回归确认DHT11持续`status=OK`、heartbeat递增、PA1约每秒翻转、ST7735S画面正常、USART1无持续乱码；没有观察到异常声音、异味、明显发热或复位。
- 编码器运行中断线、扇叶、机械负载、堵转和长时间温升测试均未执行，不把这些项目描述为通过。
- 功能分支最终完整Rebuild结果为Code=28778、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)，Build Time 16秒。
- 最终HEX SHA-256为`EB81C1253245E82124DA8521D648EFC3183B8CEFEA7628E532ACD5AAB96AC8A4`，与Task 8关闭临时自检后的正式固件一致。
- 结论：M7单键事件识别、AUTO/MANUAL/BACKFLOW模式、LOW/HIGH预选、运行许可、故障锁存/清除、M6闭环复用和所有已执行空载停止路径通过；未执行项目继续保留为后续限制。

### 2026-08-17：M7最终审查补充自检

- 最终审查发现原双击自检只检查最后的`DOUBLE`事件，不能证明中间过程没有夹带`SHORT`；原Tick回绕自检也只覆盖消抖和长按。
- `BSP_Key_TestDouble()`现对两次点击的每个中间调用明确断言`BSP_KEY_EVENT_NONE`，只有第二次稳定释放允许返回`BSP_KEY_EVENT_DOUBLE`。
- Tick回绕自检拆分为LONG、SHORT和DOUBLE三项，分别覆盖跨32位回绕的长按计时、350ms单击等待边界和双击窗口，并继续验证双击过程中不夹带单击。
- 临时设置`APP_M7_SELF_TEST_ENABLED=1U`后完整Rebuild为Code=30910、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)。断开VM烧录并复位，串口明确输出`M7 self-test PASSED`。
- 板端自检通过后已将`APP_M7_SELF_TEST_ENABLED`恢复为`0U`；正式固件仍保留生产按键映射，不在每次启动时重复运行自检。
- 恢复正式配置后的完整Rebuild为Code=28778、RO-data=1422、RW-data=168、ZI-data=39672，0 Error(s)、0 Warning(s)；HEX SHA-256仍为`EB81C1253245E82124DA8521D648EFC3183B8CEFEA7628E532ACD5AAB96AC8A4`。
