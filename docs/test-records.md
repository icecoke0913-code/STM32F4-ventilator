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
| M2-T1 | ST7735S颜色、方向和刷新稳定 | 未测试 |
| M3-T1 | DHT11连续读取和断线处理 | 未测试 |
| M3-T2 | MQ-2 ADC范围、滤波和预热变化 | 未测试 |
| M4-T1 | 电机正转、停止和PWM调速 | 未测试 |
| M5-T1 | 编码器方向、计数和RPM标定 | 未测试 |
| M6-T1 | 190 RPM稳态误差 | 未测试 |
| M6-T2 | 220 RPM稳态误差 | 未测试 |
| M6-T3 | 目标阶跃响应和堵转保护 | 未测试 |
| M7-T1 | 单键短按、双击和长按识别 | 未测试 |
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
