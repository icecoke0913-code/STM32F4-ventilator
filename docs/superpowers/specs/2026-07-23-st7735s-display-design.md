# STM32F407 ST7735S 显示模块设计

## 1. 目标与范围

M2为SmartHood工程增加外接1.8英寸ST7735S显示模块。屏幕使用竖屏128×160、RGB565颜色格式，通过STM32F407VET6的SPI2和独立GPIO驱动。

M2只完成可复用的基础显示驱动和硬件校准，不实现中文、图片、DMA、正式多页面UI或独立UiTask。

## 2. 硬件接口

不使用开发板P2 TFT插座。屏幕通过扩展排针和杜邦线连接：

| TFT引脚 | 功能 | STM32连接 | 电气说明 |
|---|---|---|---|
| BLK | 背光控制 | PD4 | 初始Low，初始化后置为有效电平；首次上板确认极性 |
| CS | SPI片选 | PD7 | 低电平有效 |
| DC | 命令/数据选择 | PD6 | Low为命令，High为参数或像素数据 |
| RST | 硬件复位 | PD5 | 低电平有效 |
| SDA | SPI数据输入 | PB15 / SPI2_MOSI | 仅STM32向屏幕发送 |
| SCL | SPI时钟 | PB13 / SPI2_SCK | SPI时钟信号 |
| VDD | 模块电源 | 开发板3.3V | 禁止连接5V |
| GND | 电源地 | 开发板GND | 与STM32、ST-Link共地 |

屏幕没有MISO信号。丝印SDA/SCL在本模块上表示SPI数据和SPI时钟，不是I2C总线。

## 3. 供电与接线安全

- 开发板由USB-C供电。
- TFT由开发板3.3V供电。
- ST-Link只连接SWDIO、SWCLK和GND，不连接ST-Link的3.3V或5V。
- 所有接线和调整都在完全断电后进行。
- M2只连接TFT，不连接MQ-2、DHT11、TB6612、电机或编码器。
- SPI和控制线尽量短于20cm，并确保GND接触可靠。

## 4. CubeMX外设配置

SPI2使用硬件主机发送模式，初始配置为：

```text
Mode: Master, Transmit Only
Data Size: 8 bit
Clock Polarity: Low
Clock Phase: 1 Edge
NSS: Software
First Bit: MSB First
Baud Rate Prescaler: 8
SPI Clock: 42 MHz / 8 = 5.25 MHz
CRC: Disabled
DMA: Disabled
SPI Interrupt: Disabled
```

控制GPIO初始状态：

```text
PD7 / TFT_CS:  High
PD6 / TFT_DC:  Low
PD5 / TFT_RST: High
PD4 / TFT_BLK: Low
```

四个GPIO均使用推挽输出、无上下拉、低速或中速。SPI引脚由CubeMX配置为对应复用功能。

## 5. 软件结构

新增文件：

```text
firmware/SmartHood/BSP/Inc/bsp_st7735s.h
firmware/SmartHood/BSP/Src/bsp_st7735s.c
firmware/SmartHood/BSP/Inc/fonts.h
firmware/SmartHood/BSP/Src/fonts.c
```

职责边界：

- `bsp_st7735s`封装SPI发送、GPIO控制、初始化、设置绘图窗口、颜色填充、像素、矩形和ASCII文本。
- `fonts`只保存ASCII字模数据，不访问HAL、SPI或GPIO。
- `app_tasks.c`只调用显示测试接口，不直接操作SPI寄存器或TFT控制脚。
- M2只有默认任务访问屏幕，不创建互斥量；正式UiTask加入后再保护显示资源。

第一版使用HAL阻塞发送，底层调用`HAL_SPI_Transmit()`。不使用DMA、中断或双缓冲。

## 6. 初始化与数据格式

初始化顺序：

1. 保持CS为High、BLK为Low。
2. 对RST执行硬件复位并满足控制器延时要求。
3. 发送`SWRESET (0x01)`并等待复位完成。
4. 发送`SLPOUT (0x11)`退出休眠。
5. 配置ST7735S帧率、电源和伽马寄存器。
6. 发送`COLMOD (0x3A) = 0x05`，选择16位RGB565。
7. 通过`MADCTL (0x36)`选择竖屏方向及RGB/BGR顺序。
8. 发送`DISPON (0x29)`打开显示。
9. 全屏填充黑色。
10. 打开背光。

RGB565基准颜色：

```text
Black: 0x0000
White: 0xFFFF
Red:   0xF800
Green: 0x07E0
Blue:  0x001F
```

每个像素先发送高字节，再发送低字节。

## 7. 坐标与方向校准

逻辑坐标范围：

```text
左上角: (0, 0)
右下角: (127, 159)
```

绘图窗口使用：

```text
CASET (0x2A): X范围
RASET (0x2B): Y范围
RAMWR (0x2C): 开始写像素
```

首轮将X/Y offset设为0。根据实物结果调整：

- 红蓝互换：修改MADCTL的RGB/BGR位。
- 镜像或倒置：修改MADCTL的MX、MY、MV位。
- 边框缺失或整体偏移：修改X/Y offset。

偏移和MADCTL最终值必须记录在测试文档中，不能仅依赖网上同型号模块参数。

## 8. 错误处理与运行约束

- SPI发送使用有限超时，不能永久阻塞FreeRTOS任务。
- 初始化函数返回成功或失败状态。
- 初始化失败时保持背光关闭，并通过USART1输出错误日志。
- 应用层检测到初始化失败后不继续执行全屏刷新。
- M2保留M1串口心跳，用于观察显示操作是否造成任务失联或异常复位。

## 9. 测试顺序

1. CubeMX增加SPI2和四个控制GPIO。
2. 重新生成代码，检查M1 USER CODE和手写App文件仍然存在。
3. TFT未接线时完成Keil空载编译。
4. 断电连接TFT。
5. 验证PD4背光控制极性。
6. 初始化屏幕并显示红、绿、蓝、白、黑全屏。
7. 显示四角彩色标记和一圈白色边框。
8. 显示`SmartHood`、`ST7735S OK`和`128x160` ASCII文本。
9. 在屏幕测试期间观察USART1心跳持续输出。

## 10. M2验收标准

- Keil编译0错误、0警告。
- 背光可由PD4控制。
- 红、绿、蓝颜色对应正确。
- 显示方向为竖屏且没有镜像或倒置。
- 四角和边框完整，无明显坐标偏移。
- ASCII文本清晰可读。
- 串口心跳在显示操作期间持续运行。
- 重复刷新时不出现白屏、花屏或异常复位。

## 11. 明确排除项

M2不实现中文字体、位图图片、SPI DMA、双缓冲、正式多页面UI、独立UiTask或背光PWM。这些功能在基础驱动通过后按后续里程碑需要增加。

## 12. M9 LVGL移植预留

LVGL不在M2实现，统一留到M9正式UI阶段。M2底层驱动必须保留以下扩展能力：

- RGB565作为屏幕像素格式，与LVGL显示颜色格式保持一致。
- 提供任意矩形绘图窗口和连续像素写入能力，供后续LVGL显示刷新回调调用。
- SPI、CS、DC、RST和BLK操作全部封装在BSP内部，LVGL适配层不直接依赖具体GPIO。
- 阻塞式SPI接口可以在不改变上层绘图接口的前提下替换为SPI DMA。
- M9新增LVGL显示缓冲区、刷新完成通知、UiTask和显示资源同步。
- M2自带ASCII字模只用于硬件验收；M9使用LVGL字体系统，两者不互相依赖。

M9开始前再根据Keil/ARMCC兼容性、Flash/RAM占用和当时的稳定版本确定具体LVGL版本，不在M2提前锁定版本号。
