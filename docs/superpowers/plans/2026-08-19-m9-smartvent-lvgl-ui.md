# M9 SmartVent LVGL中文状态面板实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在保持`SmartHood`内部工程名不变的前提下，为智能通风边缘控制系统增加LVGL 8.3.11中文单页状态面板、线程安全UI快照和分层故障显示。

**Architecture:** MotorTask作为唯一UI快照生产者，每50 ms以0超时发布完整`UiSnapshot_t`；UiTask作为唯一LVGL/ST7735S运行期访问者，每5 ms处理LVGL、每500 ms应用快照。显示刷新沿用M2的RGB565矩形写入接口和SPI2阻塞发送，只有测得阻塞影响控制周期时才单独升级DMA。

**Tech Stack:** STM32F407VET6, STM32CubeMX 6.16.1, STM32CubeF4 1.28.3, Keil MDK-ARM V5 ARMCC5.06, FreeRTOS/CMSIS-RTOS v2, LVGL 8.3.11, ST7735S SPI2, C, NotoSansSC-Regular OFL-1.1 subset font.

---

## 文件结构与职责

新增第三方代码：`firmware/SmartHood/Middlewares/Third_Party/lvgl/`，只保留LVGL核心、必要绘制模块、字体支持和`lv_conf.h`；不加入Demo、示例、图片和未使用控件。

新增应用文件：

```text
firmware/SmartHood/App/Inc/ui_state.h
firmware/SmartHood/App/Src/ui_state.c
firmware/SmartHood/App/Inc/ui_screen.h
firmware/SmartHood/App/Src/ui_screen.c
firmware/SmartHood/App/Inc/lv_port_disp.h
firmware/SmartHood/App/Src/lv_port_disp.c
firmware/SmartHood/App/Inc/ui_font.h
firmware/SmartHood/App/Src/ui_font.c
firmware/SmartHood/App/Inc/ui_state_selftest.h
firmware/SmartHood/App/Test/ui_state_selftest.c
```

修改`app_tasks.c`、`app_tasks.h`、`freertos.c`、`FreeRTOSConfig.h`（仅当实测堆不足）、`SmartHood.uvprojx`、`docs/project-guide.md`和`docs/test-records.md`。所有新增C代码必须有中文模块、接口、数据流和错误处理注释。

### Task 1: 固定依赖并建立可回退基线

**Files:** Create `Middlewares/Third_Party/lvgl/` and `lv_conf.h`; modify `MDK-ARM/SmartHood.uvprojx` and `.gitignore`.

- [ ] **Step 1: Rebuild M8A基线**

```powershell
& 'D:\Keil5\UV4\UV4.exe' -r 'D:\Keil5 prj\stm32f4\firmware\SmartHood\MDK-ARM\SmartHood.uvprojx' -j0 -o 'D:\Keil5 prj\stm32f4\firmware\SmartHood\MDK-ARM\m9_baseline.log'
Get-Content 'D:\Keil5 prj\stm32f4\firmware\SmartHood\MDK-ARM\m9_baseline.log'
```

Expected: `0 Error(s), 0 Warning(s)`；记录Code、RO-data、RW-data、ZI-data和HEX SHA-256。

- [ ] **Step 2: 导入并核验LVGL 8.3.11**：从LVGL官方8.3.11发行包复制`src/`和配置模板，核对`lv_version.h`为8.3.11；不复制`examples/`和`demos/`。
- [ ] **Step 3: 创建最小`lv_conf.h`**：固定`LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0`、`LV_MEM_CUSTOM=0`、`LV_MEM_SIZE=(16U*1024U)`、`LV_USE_LOG=0`、`LV_USE_ANIM=0`、`LV_USE_IMG=0`、`LV_USE_CANVAS=0`、`LV_USE_TABLE=0`、`LV_USE_LIST=0`、`LV_USE_DROPDOWN=0`、`LV_USE_KEYBOARD=0`、`LV_USE_LABEL=1`、`LV_USE_CONT=1`、`LV_USE_THEME_DEFAULT=0`，每个非默认开关写中文原因。
- [ ] **Step 4: 更新Keil工程**：增加LVGL组，加入与配置对应的核心、draw、font、misc、hal、widgets源文件；IncludePath增加`..\\Middlewares\\Third_Party\\lvgl`和`..\\Middlewares\\Third_Party\\lvgl\\src`。
- [ ] **Step 5: Rebuild并提交**：预期0错误0警告；`git commit -m "build: add minimal LVGL 8.3.11 integration"`。

### Task 2: 建立ST7735S局部刷新端口

**Files:** Create `App/Inc/lv_port_disp.h` and `App/Src/lv_port_disp.c`; modify Keil工程。

- [ ] **Step 1: 声明接口**：

```c
bool LvPortDisp_Init(void);
bool LvPortDisp_HasError(void);
uint32_t LvPortDisp_GetLastFlushMs(void);
```

接口只能由UiTask调用，刷新回调在所有BSP路径都必须调用`lv_disp_flush_ready()`。
- [ ] **Step 2: 定义缓冲区**：`static lv_color_t ui_draw_buffer[128U * 20U];`，使用`lv_disp_draw_buf_init()`、`lv_disp_drv_init()`，设置`hor_res=128`、`ver_res=160`和`flush_cb`。
- [ ] **Step 3: 实现刷新回调**：把`lv_area_t`裁剪到0..127/0..159，计算包含端点的width、height和pixel_count，调用`BSP_ST7735S_SetAddressWindow()`与`BSP_ST7735S_WritePixels()`；失败标记并每1000 ms限频日志，保存最大flush耗时。
- [ ] **Step 4: 端口自检**：用1×1和128×20区域验证边界计算，不在正式启动发送额外画面。
- [ ] **Step 5: Rebuild并提交**：预期0错误0警告；`git commit -m "feat: add LVGL ST7735S display port"`。

### Task 3: 建立线程安全UI快照

**Files:** Create `App/Inc/ui_state.h`, `App/Src/ui_state.c`, `App/Inc/ui_state_selftest.h`、`App/Test/ui_state_selftest.c`；modify Keil工程。

- [ ] **Step 1: 定义类型和接口**：

```c
typedef struct {
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

bool UiState_Init(void);
bool UiState_Publish(const UiSnapshot_t *snapshot);
bool UiState_Read(UiSnapshot_t *snapshot);
```

发布和读取均使用0超时互斥量；发布失败不得阻塞MotorTask。
- [ ] **Step 2: 写纯数据自检**：覆盖初始STOP/AUTO/LOW、正常DHT11、过期DHT11、MANUAL HIGH、BACKFLOW预留、ENCODER_TIMEOUT、PWM 0/90和非法枚举安全映射；不访问HAL/SPI/GPIO/电机。
- [ ] **Step 3: 临时打开`APP_M9_UI_SELF_TEST_ENABLED 1U`**：MotorTask启动前输出`M9 UI state self-test PASSED`，失败保持STOP；通过后恢复0U。
- [ ] **Step 4: Rebuild并提交**：预期自检通过且0错误0警告；`git commit -m "feat: add thread-safe SmartVent UI snapshot"`。

### Task 4: 生成精简中文字体并实现A布局

**Files:** Create `BSP/Inc/ui_font.h`、`BSP/Src/ui_font.c`、`Assets/fonts/NotoSansSC-OFL-1.1.txt`、`Assets/fonts/README.md`、`App/Inc/ui_screen.h`、`App/Src/ui_screen.c`；modify Keil工程。

- [ ] **Step 1: 固定字体**：取得NotoSansSC-Regular版本及OFL-1.1许可证，字符集合固定为：`运行 待机 故障 自动模式 手动模式 防回流 预留 低档 高档 停止 温度 湿度 目标计数 反馈计数 输出 系统正常 传感器失联 编码器故障 功能预留 数据不可用`。
- [ ] **Step 2: 生成字模**：使用LVGL字体转换工具生成14 px、2 bpp、仅上述字符的`ui_font.c/.h`；在README记录工具版本、命令和缺字检查。
- [ ] **Step 3: 声明控件接口**：

```c
bool UiScreen_Create(void);
bool UiScreen_ApplySnapshot(const UiSnapshot_t *snapshot);
```

创建顶部标题/状态、模式/档位、温湿度、目标/反馈/PWM和底部状态栏控件。
- [ ] **Step 4: 实现映射**：显式switch处理枚举非法值；整数拆分温湿度，禁用浮点printf；STOP/FAULT目标计数和PWM为0；反馈使用最近完整500 ms平均值。
- [ ] **Step 5: 实现状态优先级**：编码器故障 > UI数据不可用 > DHT11失联 > 防回流功能预留 > 系统正常；颜色分别红、黄、黄、橙、灰蓝，顶部同步运行/待机/故障。
- [ ] **Step 6: Rebuild并提交**：预期0错误0警告；`git commit -m "feat: add SmartVent Chinese status screen"`。

### Task 5: 创建UiTask并接入生产路径

**Files:** modify `Core/Src/freertos.c`、`App/Inc/app_tasks.h`、`App/Src/app_tasks.c`、Keil工程；仅当实测不足才改`FreeRTOSConfig.h`。

- [ ] **Step 1: 增加任务属性**：

```c
const osThreadAttr_t uiTask_attributes = {
  .name = "uiTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
```

增加`StartUiTask()`，应用层声明`App_UiTask(void *argument)`。
- [ ] **Step 2: 实现隔离启动**：UiTask依次调用`BSP_ST7735S_Init()`、`lv_init()`、`LvPortDisp_Init()`、`UiScreen_Create()`；失败则关闭背光、限频日志、每1000 ms等待，不调用MotorTask。
- [ ] **Step 3: 实现节拍**：每5 ms调用`lv_timer_handler()`，每500 ms读取完整快照并调用`UiScreen_ApplySnapshot()`；不持有快照锁执行SPI。
- [ ] **Step 4: MotorTask发布**：每50 ms控制计算结束后组装完整`UiSnapshot_t`并调用`UiState_Publish()`；发布失败只累计诊断计数，反馈字段只在500 ms统计窗口替换。
- [ ] **Step 5: 关闭旧颜色自检**：将DefaultTask的`App_RunDisplayTest()`放入`APP_DISPLAY_SELF_TEST_ENABLED`条件编译，正式开关为0，启动后直接显示SmartVent。
- [ ] **Step 6: Rebuild并提交**：记录剩余堆和所有任务栈高水位；预期0错误0警告；`git commit -m "feat: add isolated SmartVent UI task"`。

### Task 6: VM断开验收

**Files:** modify `docs/test-records.md`；create `docs/superpowers/plans/m9-vm-disconnected-checklist.md`。

- [ ] **Step 1:** 确认`APP_M9_UI_SELF_TEST_ENABLED=0U`和`APP_DISPLAY_SELF_TEST_ENABLED=0U)，完成新鲜Rebuild并保存Code/RAM/HEX SHA-256。
- [ ] **Step 2:** 保持VM断开，TFT使用已验收3.3V，所有改线完全断电，不带扇叶/机械负载。
- [ ] **Step 3:** 烧录后核对SmartVent界面、温湿度、模式/档位、目标/反馈计数、PWM和PA0三事件；DHT11断开必须断电改线，等待超过6000 ms确认黄色失联，再恢复确认自动恢复。
- [ ] **Step 4:** 进入防回流确认“功能预留”和STOP；验证10分钟无白屏、花屏、复位、串口失联、按键漏识别。
- [ ] **Step 5:** 把每项串口/屏幕证据写入`docs/test-records.md`，提交`git commit -m "test: record SmartVent UI disconnected-VM validation"`。

### Task 7: VM接通空载和稳定性验收

**Files:** modify `docs/test-records.md`、`docs/project-guide.md`；create `docs/superpowers/plans/m9-stability-record.md`。

- [ ] **Step 1:** 完全断电接通VM；禁止扇叶、机械负载、堵转和带电插拔，上电先核对PWM 0。
- [ ] **Step 2:** 验证AUTO触发、MANUAL低/高档、长按停止、BACKFLOW预留STOP和RST停止；同时核对屏幕与串口。
- [ ] **Step 3:** 30分钟运行记录heartbeat、DHT11恢复、flush最大耗时、MotorTask日志、UiTask栈和剩余堆。
- [ ] **Step 4:** 30分钟无异常后执行2小时测试；记录模式变化、故障提示和资源余量。
- [ ] **Step 5:** 只有出现PA0/heartbeat延迟、50 ms控制抖动、500 ms刷新超时或可重复长阻塞，才创建DMA子计划；否则记录阻塞式SPI最大耗时。
- [ ] **Step 6:** 提交`git commit -m "test: record SmartVent UI stability validation"`。

### Task 8: 企业化对外文档与发布检查

**Files:** Create `README.md`；modify `docs/project-guide.md`、`docs/test-records.md`、`.gitignore`。

- [ ] **Step 1:** README标题固定为`SmartVent Edge Controller / 智能通风边缘控制系统`；章节为项目概述、架构、功能、硬件、任务数据流、控制策略、UI、验证、限制、构建烧录；详细教学过程链接到docs。
- [ ] **Step 2:** 不声称量产/工业认证、EMC/安规、可靠RPM精度、MQ-2浓度、真实防回流、扇叶/负载/堵转或温升已完成；使用“面向工程化的嵌入式智能通风边缘控制原型”。
- [ ] **Step 3:** 最终运行`git diff --check`、Keil新鲜完整Rebuild、`git status --short --branch`、`git log -8 --oneline --decorate`；确认正式自检开关为0U、main与远端同步。
- [ ] **Step 4:** 提交`git commit -m "docs: publish SmartVent engineering project"`并用固定DNS解析推送main。

## 自检清单

- [ ] 设计范围全部由Task 1～8覆盖。
- [ ] `UiSnapshot_t`字段在Task 3定义后，Task 4/5使用完全相同名称。
- [ ] `LvPortDisp_Init()`、`UiScreen_Create()`、`UiState_Publish()`、`UiState_Read()`声明和调用一致。
- [ ] 16 KiB池、5120字节缓冲、2048字节UiTask栈、5/500 ms节拍在各任务一致。
- [ ] 状态栏优先级始终为“编码器故障 > UI数据不可用 > 传感器失联 > 防回流预留 > 正常”。
- [ ] 未将RPM、MQ-2、真实防回流、扇叶、负载、堵转或安规写成完成事实。
- [ ] 没有TBD、TODO、“适当处理”或“根据需要实现”等占位语句。
- [ ] 所有新增代码任务都要求中文注释、ARMCC5编译和明确预期结果。
