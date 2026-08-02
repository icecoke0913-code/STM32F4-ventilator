# Chinese Comments for Custom Code Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为SmartHood全部自编App/BSP代码和Core的USER CODE区域补充结构化中文注释，并用编译尺寸与HEX哈希证明固件行为没有改变。

**Architecture:** 注释改造按UTF-8试验、App、DHT11、ST7735S/字模、FreeRTOS USER CODE分批进行。每一批只允许修改注释、空白和文件编码，用户在Keil中确认中文显示并构建，助手检查源码差异、程序大小和Git状态。

**Tech Stack:** C、STM32CubeF4 HAL、FreeRTOS CMSIS_V2、STM32CubeMX 6.16.1、Keil μVision 5.24、ARMCC 5.06、Git、UTF-8。

---

## 文件与职责

```text
App/Inc/app_tasks.h
    默认任务和传感器任务的应用层入口说明。

App/Inc/debug_log.h
    线程安全调试日志接口说明。

App/Src/app_tasks.c
    TFT启动自检、心跳、按键和DHT11周期任务流程。

App/Src/debug_log.c
    CMSIS-RTOS2互斥量、格式化缓冲区和USART1发送逻辑。

BSP/Inc/bsp_dht11.h
BSP/Src/bsp_dht11.c
    DHT11状态/数据接口、GPIO方向切换、TIM5微秒时序和校验。

BSP/Inc/bsp_st7735s.h
BSP/Src/bsp_st7735s.c
    ST7735S尺寸/颜色/API、SPI命令数据、初始化、绘图和边界处理。

BSP/Inc/fonts.h
BSP/Src/fonts.c
    5×7 ASCII字模格式、查找与未知字符处理。

Core/Src/freertos.c
    仅注释USER CODE内的头文件依赖、日志初始化和任务委托。
```

禁止修改`Drivers/`、`Middlewares/`、CMSIS、FreeRTOS内核和Core生成区。

## 回归判据

注释前已确认：

```text
Program Size:
Code=24002 RO-data=1286 RW-data=156 ZI-data=39524

SmartHood.hex SHA-256:
0DC8FC5B0E3CDCCAF4860CBD36F1DF81B97263A7F2E895ED10C4A2DDCA7C96E8

Keil:
0 Error(s), 0 Warning(s)
```

注释后必须保持相同Program Size。HEX哈希也应相同；若哈希不同，先检查是否存在非注释源码差异，不以“只是注释”作为假设继续。

---

### Task 1: UTF-8中文注释小文件试验

**Files:**
- Modify: `firmware/SmartHood/App/Inc/debug_log.h`
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood/SmartHood.build_log.htm`

- [ ] **Step 1: 助手记录基线状态**

核对当前分支、工作区和基线哈希：

```text
Branch: codex/feature-m3a-dht11
HEAD: b76f8ec或其后仅含注释设计/计划的提交
HEX SHA-256: 0DC8FC5B0E3CDCCAF4860CBD36F1DF81B97263A7F2E895ED10C4A2DDCA7C96E8
```

- [ ] **Step 2: 助手为debug_log.h添加试验注释**

注释内容必须使用UTF-8，并保持接口声明不变：

```c
/**
 * @file debug_log.h
 * @brief 线程安全调试日志模块的公共接口。
 *
 * 本模块由应用任务调用，通过USART1输出格式化文本。
 * 互斥量的创建与具体发送过程由debug_log.c负责。
 */

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdbool.h>

/**
 * @brief 创建保护调试串口的互斥量。
 * @return 创建成功或已经初始化时返回true，否则返回false。
 */
bool DebugLog_Init(void);

/**
 * @brief 按printf格式生成日志并通过USART1阻塞发送。
 * @param format printf风格的格式字符串，后面可跟可变参数。
 */
void DebugLog_Printf(const char *format, ...);

#endif
```

- [ ] **Step 3: 用户检查Keil中文显示**

在Keil中重新打开或刷新`debug_log.h`，确认：

```text
中文可正常阅读
没有显示为问号、方框或乱码
英文标识符和缩进未改变
```

如果显示乱码，停止批量注释，先调整Keil文件编码显示；不得将该文件另存为GBK后继续。

- [ ] **Step 4: 用户执行Build**

执行：

```text
F7 / Build Target
```

预期：

```text
0 Error(s), 0 Warning(s)
Program Size保持Code=24002、RO-data=1286、RW-data=156、ZI-data=39524
```

- [ ] **Step 5: 助手提交UTF-8试验检查点**

提交信息：

```text
docs: verify UTF-8 Chinese source comments
```

---

### Task 2: 注释App公共接口和DebugLog实现

**Files:**
- Modify: `firmware/SmartHood/App/Inc/app_tasks.h`
- Modify: `firmware/SmartHood/App/Src/debug_log.c`

- [ ] **Step 1: 注释app_tasks.h**

保持两个函数声明不变，添加以下职责说明：

```c
/**
 * @file app_tasks.h
 * @brief SmartHood应用层FreeRTOS任务入口。
 *
 * CubeMX生成的任务入口只负责调用这里声明的应用函数，
 * 业务逻辑因此不会在CubeMX重新生成代码时被覆盖。
 */

/**
 * @brief 默认任务：执行显示自检，并周期处理心跳、LED和按键日志。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
void App_DefaultTask(void *argument);

/**
 * @brief 传感器任务：周期读取DHT11并输出采集状态。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
void App_SensorTask(void *argument);
```

- [ ] **Step 2: 注释debug_log.c模块状态**

在包含区之后、互斥量定义之前加入：

```c
/*
 * 两个任务可能同时输出日志，因此使用同一把CMSIS-RTOS2互斥量
 * 串行化“格式化 + UART发送”的完整过程，避免日志交叉或HAL忙状态。
 */
static osMutexId_t debug_log_mutex = NULL;
```

原互斥量定义只能保留一份，不得重复声明。

- [ ] **Step 3: 注释DebugLog_Init**

在函数前加入：

```c
/**
 * @brief 创建日志互斥量。
 * @return 初始化成功或已经初始化时返回true，创建失败返回false。
 *
 * 该函数在MX_FREERTOS_Init()中、任务创建之前调用，
 * 从而保证任何任务开始输出日志时互斥量已经存在。
 */
```

在已有非NULL判断前加入：

```c
/* 允许重复调用初始化函数，但不重复分配内核对象。 */
```

- [ ] **Step 4: 注释DebugLog_Printf**

在函数前加入：

```c
/**
 * @brief 格式化并发送一条线程安全的USART1调试日志。
 * @param format printf风格格式字符串，后面可跟可变参数。
 *
 * 日志缓冲区固定为160字节；过长文本会被截断，
 * UART使用100 ms阻塞超时。函数不向调用者返回发送结果。
 */
```

在三个逻辑阶段分别加入：

```c
/* 未初始化或格式字符串无效时直接忽略本次日志。 */
/* 获取互斥量后，其他任务必须等待当前整条日志发送完成。 */
/* 限制发送长度，防止vsnprintf返回的原始长度超过缓冲区。 */
```

释放互斥量前加入：

```c
/* 无论格式化结果是否有效，都必须释放已经获得的互斥量。 */
```

- [ ] **Step 5: 用户执行Build**

预期0错误、0警告，Program Size不变。

---

### Task 3: 注释App任务流程

**Files:**
- Modify: `firmware/SmartHood/App/Src/app_tasks.c`

- [ ] **Step 1: 注释显示自检函数**

在`App_RunDisplayTest()`前加入：

```c
/**
 * @brief 执行一次ST7735S启动自检。
 * @return 初始化和全部绘图操作成功时返回true，任一步失败返回false。
 *
 * 自检依次显示纯色画面，再绘制边框、四色角标和文本，
 * 用于同时检查SPI通信、方向、RGB顺序、坐标范围和字体绘制。
 */
```

在纯色序列、边框、角标和文本四个代码段前分别加入：

```c
/* 纯色切换用于检查RGB565颜色通道和全屏写入。 */
/* 单像素白框用于确认四条边界和坐标偏移。 */
/* 四色角标用于同时核对方向、角点位置和颜色顺序。 */
/* 英文与数字文本用于检查5×7字模和字符串绘制。 */
```

- [ ] **Step 2: 注释默认任务**

在`App_DefaultTask()`前加入：

```c
/**
 * @brief 运行M1基础功能和M2显示自检的默认任务。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 任务启动时只执行一次TFT自检；随后每秒翻转PA1、读取PA0，
 * 并输出心跳序号、按键状态和HAL毫秒Tick。
 */
```

在TFT失败分支前说明关闭背光用于明确显示初始化失败；在循环前说明1秒周期。

- [ ] **Step 3: 注释传感器任务**

在`App_SensorTask()`前加入：

```c
/**
 * @brief 每约2秒读取一次DHT11并输出温湿度或错误状态。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 首次读取前等待2秒满足DHT11上电稳定时间；读取失败不会终止任务，
 * 下一个周期会自动重试，因此传感器重新接入后可以自行恢复。
 */
```

在初始化失败、首次等待、状态分支和数值拆分处加入：

```c
/* TIM5无法启动属于不可恢复的基础外设故障，保留任务但停止采集。 */
/* 等待DHT11上电稳定，避免启动后立即读取造成无效响应。 */
/* 使用扩大10倍的整数拆分整数位和小数位，避免浮点格式化。 */
/* TIMEOUT和CHECKSUM_ERROR只记录状态，下个周期继续尝试。 */
```

- [ ] **Step 4: 用户执行Rebuild**

预期0错误、0警告，Program Size不变。

- [ ] **Step 5: 助手提交App注释检查点**

提交信息：

```text
docs: add Chinese comments to application modules
```

---

### Task 4: 注释DHT11接口与驱动

**Files:**
- Modify: `firmware/SmartHood/BSP/Inc/bsp_dht11.h`
- Modify: `firmware/SmartHood/BSP/Src/bsp_dht11.c`

- [ ] **Step 1: 注释DHT11状态与数据结构**

在枚举和结构体前加入：

```c
/** DHT11一次读取的结果状态。 */
```

枚举成员使用行尾注释：

```c
DHT11_STATUS_OK = 0,          /**< 数据完整且校验和正确。 */
DHT11_STATUS_TIMEOUT,         /**< 等待响应或数据边沿超时。 */
DHT11_STATUS_CHECKSUM_ERROR   /**< 收到40位数据但校验和错误。 */
```

结构体字段使用：

```c
int16_t temperature_x10; /**< 摄氏温度扩大10倍，可表示负温度。 */
uint16_t humidity_x10;   /**< 相对湿度百分数扩大10倍。 */
```

- [ ] **Step 2: 注释公开接口**

```c
/**
 * @brief 将PD0恢复为输入上拉并启动TIM5微秒计数器。
 * @return TIM5启动成功返回true，否则返回false。
 */
bool BSP_DHT11_Init(void);

/**
 * @brief 按DHT11单总线协议读取一次温湿度。
 * @param data 有效输出结构指针；只有返回OK时才更新其内容。
 * @return OK、TIMEOUT或CHECKSUM_ERROR。
 */
DHT11_Status_t BSP_DHT11_Read(DHT11_Data_t *data);
```

- [ ] **Step 3: 注释时序常量**

在驱动宏定义前加入：

```c
/* DHT11协议时序单位均为微秒，TIM5以1 MHz计数提供时间基准。 */
```

每个宏添加行尾说明：18 ms主机启动、30 μs释放、120 μs边沿保护、50 μs位阈值、5字节和40位。

- [ ] **Step 4: 注释GPIO与时间辅助函数**

为`DHT11_SetOutputLow()`、`DHT11_SetInput()`、`DHT11_GetTimeUs()`、`DHT11_DelayUs()`和`DHT11_WaitForPin()`添加函数说明。

必须明确：

```text
开漏输出只主动拉低，写SET表示释放总线
输入模式保留内部上拉，使断线状态保持高电平
无符号计数差支持TIM5回绕
WaitForPin所有循环都有超时
```

- [ ] **Step 5: 注释完整读取状态机**

在`BSP_DHT11_Read()`前加入函数文档，并在内部按下列阶段添加块注释：

```c
/* 阶段1：主机拉低总线至少18 ms，通知DHT11开始一次传输。 */
/* 阶段2：保存中断状态并释放总线；30 μs后开始捕获传感器响应。 */
/* 阶段3：确认约80 μs低电平和80 μs高电平响应序列。 */
/* 阶段4：读取40位数据，高电平宽度大于50 μs判定为1。 */
/* 阶段5：先恢复中断和GPIO，再根据采集结果返回错误。 */
/* 阶段6：验证前4字节累加校验和，并转换温湿度。 */
/* 只有完整成功后才写入调用者结构，失败时保留旧数据。 */
```

- [ ] **Step 6: 用户执行Rebuild**

预期0错误、0警告，Program Size不变。

- [ ] **Step 7: 助手提交DHT11注释检查点**

提交信息：

```text
docs: explain DHT11 timing and recovery logic
```

---

### Task 5: 注释ST7735S公共接口与底层传输

**Files:**
- Modify: `firmware/SmartHood/BSP/Inc/bsp_st7735s.h`
- Modify: `firmware/SmartHood/BSP/Src/bsp_st7735s.c`

- [ ] **Step 1: 注释尺寸和RGB565颜色宏**

说明：

```text
逻辑竖屏尺寸为128×160
颜色常量为RGB565：红5位、绿6位、蓝5位
MADCTL=0xC0来自M2实物方向/颜色校准
X/Y Offset均为0来自边框与四角测试
```

- [ ] **Step 2: 注释公开绘图接口**

为以下函数添加`@brief`、参数含义和返回条件：

```text
BSP_ST7735S_Init
BSP_ST7735S_SetBacklight
BSP_ST7735S_SetAddressWindow
BSP_ST7735S_WritePixels
BSP_ST7735S_FillScreen
BSP_ST7735S_DrawPixel
BSP_ST7735S_FillRect
BSP_ST7735S_DrawChar
BSP_ST7735S_DrawString
```

坐标统一说明左上角为`(0,0)`；颜色参数为RGB565；布尔返回值表示参数、SPI发送和绘图过程是否成功。

- [ ] **Step 3: 注释SPI辅助函数**

在源文件中解释：

```text
CS低有效，事务前选中、结束后释放
DC低表示命令、DC高表示数据
HAL_SPI_Transmit使用100 ms超时
命令与数据组合函数保证CS覆盖完整事务
背光高电平点亮，硬件复位使用低脉冲
```

- [ ] **Step 4: 注释初始化序列**

将长初始化函数分为以下注释段，不修改命令或参数：

```text
硬件复位与软件复位
退出休眠
帧率设置
显示反转设置
电源与VCOM设置
正/负伽马曲线
RGB565像素格式
竖屏方向MADCTL=0xC0
正常显示与开启显示
清黑屏并点亮背光
```

- [ ] **Step 5: 注释绘图和边界处理**

明确说明：

```text
SetAddressWindow拒绝零尺寸和越界区域
端点坐标为起点加尺寸减1
RGB565按高字节在前发送
重复颜色按32像素块降低栈/发送开销
DrawChar按5列×7位读取字模
DrawString逐字符前进并在超出宽度时失败
```

- [ ] **Step 6: 用户执行Rebuild**

预期0错误、0警告，Program Size不变。

---

### Task 6: 注释5×7字模模块

**Files:**
- Modify: `firmware/SmartHood/BSP/Inc/fonts.h`
- Modify: `firmware/SmartHood/BSP/Src/fonts.c`

- [ ] **Step 1: 注释字模尺寸和接口**

在头文件说明：

```text
每个字符宽5列、高7像素
输出glyph数组长度必须为5
找到字符返回true，找不到返回false
```

- [ ] **Step 2: 注释FontGlyph5x7内部结构**

在源文件说明：

```c
/*
 * 每个字形由5个字节表示，每个字节对应一列；
 * bit0位于字符顶部，bit6位于字符底部，bit7未使用。
 */
```

只在`glyphs[]`数组前说明覆盖的ASCII字符集合，不给每条字形数据加重复注释。

- [ ] **Step 3: 注释查找逻辑**

说明函数线性查找字符、复制5列数据，并在未找到时清零输出数组后返回false；不得改变循环或数组内容。

- [ ] **Step 4: 用户执行Rebuild**

预期0错误、0警告，Program Size不变。

- [ ] **Step 5: 助手提交显示与字模注释检查点**

提交信息：

```text
docs: explain ST7735S and font rendering code
```

---

### Task 7: 注释FreeRTOS USER CODE集成点

**Files:**
- Modify only USER CODE sections: `firmware/SmartHood/Core/Src/freertos.c`

- [ ] **Step 1: 注释自编头文件依赖**

在USER CODE Includes区域加入：

```c
/* 应用任务入口与线程安全日志初始化接口，均位于CubeMX生成区之外。 */
```

- [ ] **Step 2: 注释日志初始化**

在`DebugLog_Init()`前加入：

```c
/*
 * 此时内核已经初始化但任务尚未创建，适合创建日志互斥量；
 * 初始化失败时停止系统，避免任务运行后静默丢失全部日志。
 */
```

- [ ] **Step 3: 注释任务委托**

分别在两个USER CODE任务区域加入：

```c
/* 将CubeMX任务入口委托给独立App层，防止重新生成覆盖业务逻辑。 */
```

不修改CubeMX的任务属性、句柄或函数声明。

- [ ] **Step 4: 用户执行全量Rebuild**

预期：

```text
0 Error(s), 0 Warning(s)
Code=24002 RO-data=1286 RW-data=156 ZI-data=39524
```

- [ ] **Step 5: 助手提交FreeRTOS注释检查点**

提交信息：

```text
docs: explain FreeRTOS user integration points
```

---

### Task 8: 零行为变化验证与文档收尾

**Files:**
- Verify: all modified source files
- Verify: `firmware/SmartHood/MDK-ARM/SmartHood/SmartHood.hex`
- Modify: `docs/project-guide.md`
- Modify: `docs/test-records.md`

- [ ] **Step 1: 助手检查源码差异类型**

逐文件确认差异只包含：

```text
新增或修改注释
注释所需的空行
文件编码/换行规范化
```

禁止出现标识符、常量、运算符、函数调用、数组数据或控制流变化。

- [ ] **Step 2: 用户执行最终全量Rebuild**

必须得到：

```text
0 Error(s), 0 Warning(s)
Code=24002 RO-data=1286 RW-data=156 ZI-data=39524
```

- [ ] **Step 3: 助手计算注释后HEX哈希**

执行PowerShell：

```powershell
Get-FileHash `
  'firmware\SmartHood\MDK-ARM\SmartHood\SmartHood.hex' `
  -Algorithm SHA256
```

预期哈希：

```text
0DC8FC5B0E3CDCCAF4860CBD36F1DF81B97263A7F2E895ED10C4A2DDCA7C96E8
```

- [ ] **Step 4: 用户进行最小硬件回归**

烧录后确认：

```text
启动日志和心跳正常
PA0松开/按下为key=0/1
PA1每秒翻转
ST7735S测试画面正常
```

DHT11仍保持未连接；本步骤通过后立即恢复原M3A Task 8，继续验证`DHT11 status=TIMEOUT`。

- [ ] **Step 5: 更新文档**

记录UTF-8显示、每批构建、最终Program Size、HEX哈希和硬件回归结果，并注明后续代码默认使用结构化中文注释与逻辑说明。

- [ ] **Step 6: 助手提交最终注释验收**

提交信息：

```text
docs: validate Chinese comments without firmware changes
```
