# HEX 协议集成说明

## 1. 文件结构

```
customer_app/
├── src/
│   ├── drv_uart_protocol.c    # 现有 ASCII UART 协议处理
│   └── drv_hex_protocol.c     # Hex 协议处理
├── inc/
│   ├── drv_uart_protocol.h    # ASCII UART 协议头文件
│   └── drv_hex_protocol.h     # Hex 协议头文件
└── protocols/
    ├── UART 通信协议_HEX_V2.2.pdf  # Hex 协议规范文档
    ├── HEX 协议测试指令大全.txt    # 测试指令集
    └── HEX 协议集成说明.md         # 本文档
```

## 1.5 协议格式配置

### 编译时配置

在 `hal_project.h` 中配置 UART 协议格式：

```c
// 使用 Hex 协议
#define UART_PROTOCOL_FORMAT_HEX        1
#define UART_PROTOCOL_FORMAT_ASCII      0

// 或使用 ASCII 协议
#define UART_PROTOCOL_FORMAT_HEX        0
#define UART_PROTOCOL_FORMAT_ASCII      1
```

### 注意事项

1. **二选一配置**: Hex 协议和 ASCII 协议只能启用一个
2. **编译时确定**: 配置后需要重新编译，运行时不可切换
3. **默认配置**: 当前默认为 Hex 协议格式

### 运行时行为

- **Hex 协议模式** (`UART_PROTOCOL_FORMAT_HEX=1`): 
  - 所有 UART 数据按 Hex 格式解析
  - 调用 `hex_protocol_receive_handler()` 处理
  
- **ASCII 协议模式** (`UART_PROTOCOL_FORMAT_ASCII=1`):
  - 所有 UART 数据按 ASCII 格式解析
  - 调用现有的 ASCII 协议处理逻辑

## 2. 功能说明

### 2.1 Hex 协议模块 (`drv_hex_protocol.c/.h`)

- **功能**: 解析十六进制格式的 UART 通信协议
- **协议版本**: V2.2
- **协议类型**: 
  - 基本命令 (0x01): 电机正反转控制（仅支持电机模式）
  - 扩展命令 (0x02): 电机/开关状态 + 电压电流上报
  - 管理命令 (0x09): 管理设置命令（模组→MCU）
  - 反馈命令 (0x0F): MCU 向模组发送的反馈（暂未实现）

### 2.2 工作模式

Hex 协议支持 3 种工作模式：

| 模式值 | 模式名称 | 产品类型 | 说明 |
|--------|----------|----------|------|
| 0x00 | HOLD (联动模式) | 电机产品 | 自锁控制，按压启动后持续运行 |
| 0x01 | JOG (点动模式) | 电机产品 | 按压生效，松开停止 |
| 0x02 | SWITCH (开关模式) | 开关产品 | 按 bit 控制 4 路 IO 输出 |

### 2.3 协议转换

Hex 协议 → ASCII UART 协议的转换关系：

| Hex 命令 | 模式 | 转换后 CMD | 说明 |
|---------|------|-----------|------|
| 0x01 (基本命令) | HOLD/JOG | CMD='7' | 电机正反转控制 |
| 0x02 (扩展命令) | HOLD/JOG | CMD='8' | 电机扩展参数同步 |
| 0x02 (扩展命令) | SWITCH | CMD='5' | 开关 IO 状态 + 电压电流上报 |
| 0x09 (管理命令) | - | CMD='9' | 管理设置命令（模组→MCU） |

**注意事项**:
- 基本命令 (0x01) **仅支持电机模式** (HOLD/JOG)
- 开关模式 (SWITCH) **必须使用扩展命令** (0x02)
- 管理命令 (0x09) 为**单向命令**（模组→MCU），MCU 需返回 ACK

## 3. 集成步骤

### 3.1 在 task_uart.c 中添加集成代码

在 UART 接收处理函数中添加：

```c
#include "drv_hex_protocol.h"  /* 添加头文件引用 */

void uart_receive_handler(uint8_t *data, uint8_t len)
{
    /* 判断是 Hex 协议还是 ASCII 协议 */
    if (is_hex_protocol(data, len)) {
        /* Hex 协议处理 */
        hex_protocol_receive_handler(data, len);
    } else {
        /* ASCII 协议处理（现有逻辑） */
        uart_receive_execute_command();
    }
}
```

### 3.2 在 customer_app.c 中添加初始化

如果需要在系统初始化时配置：

```c
/* 无需特殊初始化，Hex 协议模块自动工作 */
```

## 4. 使用示例

### 4.1 Hex 基本命令示例（仅电机模式）

**输入 (Hex) - 联动模式正转:**
```
AA 01 00 02 03
```
- AA: 起始符
- 01: 基本命令
- 00: 联动模式 (HOLD)
- 02: 正转
- 03: CRC 校验 (0x01+0x00+0x02=0x03)

**处理流程:**
1. `is_hex_protocol()` 识别为 Hex 协议
2. `hex_protocol_receive_handler()` 解析帧
3. 转换为 ASCII: "LF" (联动 + 正转)
4. 调用 `drv_handle_cmd_7("LF", "00", 0)`
5. 执行电机正转控制
6. 发送 Hex ACK: `AA 0F 00 02 XX`

**输入 (Hex) - 点动模式反转:**
```
AA 01 01 03 05
```
- AA: 起始符
- 01: 基本命令
- 01: 点动模式 (JOG)
- 03: 反转
- 05: CRC 校验 (0x01+0x01+0x03=0x05)

### 4.2 Hex 扩展命令示例（电机模式）

**输入 (Hex) - 联动模式正转状态上报:**
```
AA 02 00 02 01 7C 00 24 00 A6
```
- AA: 起始符
- 02: 扩展命令
- 00: 联动模式
- 02: 正转
- 017C: 电压 (380V)
- 0024: 电流 (3.6A)
- 00: 无故障
- A6: CRC 校验

**处理流程:**
1. `is_hex_protocol()` 识别为 Hex 协议
2. `hex_protocol_receive_handler()` 解析帧
3. 转换为 ASCII: "LFV380.0A036.0NN0"
4. 调用 `drv_handle_cmd_8("LFV380.0A036.0NN0", "00", 0)`
5. 执行电机电压、电流、故障码处理

### 4.3 Hex 扩展命令示例（开关模式）

**输入 (Hex) - 开关模式开启 IO1 和 IO2:**
```
AA 02 02 03 01 7C 00 24 00 A9
```
- AA: 起始符
- 02: 扩展命令
- 02: 开关模式 (SWITCH)
- 03: IO 状态 (bit0=1, bit1=1 → IO1=ON, IO2=ON)
- 017C: 电压 (380V)
- 0024: 电流 (3.6A)
- 00: 无故障
- A9: CRC 校验

**处理流程:**
1. `is_hex_protocol()` 识别为 Hex 协议
2. `hex_protocol_receive_handler()` 解析帧
3. IO 状态解析：0x03 → "0011" (IO4=OFF, IO3=OFF, IO2=ON, IO1=ON)
4. 转换为 ASCII: "V380.0A036.00011NN0"
5. 调用 `drv_handle_cmd_5("V380.0A036.00011NN0", "00", 0)`
6. 执行开关 IO 状态、电压、电流、故障码处理

**输入 (Hex) - 开关模式全开:**
```
AA 02 02 0F 01 7C 00 24 00 B4
```
- 0F: IO 状态 (bit0-3=1 → 所有 IO 开启)
- 转换为 ASCII: "V380.0A036.01111NN0"

**输入 (Hex) - 开关模式带故障上报:**
```
AA 02 02 01 01 7C 00 24 14 C9
```
- 01: 只开 IO1
- 14: 缺相 A 故障 (E-20)
- 转换为 ASCII: "V380.0A036.00001NNA"

### 4.4 Hex 管理命令示例（0x09）

**说明**: 根据《UART 通信协议_HEX_V2.2.pdf》第 3.3 节定义

**帧格式**（5 字节）:
```
AA 09 MODE VALUE CRC
```

| 字段 | 长度 | 说明 | 示例值 |
|------|------|------|--------|
| 起始符 | 1 字节 | 固定为 0xAA | 0xAA |
| 命令码 | 1 字节 | 0x09（管理设置命令） | 0x09 |
| MODE | 1 字节 | 工作模式（固定为 0x02，开关模式） | 0x02 |
| VALUE | 1 字节 | 设置值（状态记忆配置） | 0x00/0x01/0x02 |
| CRC | 1 字节 | 前 4 字节累加取低 8 位 | 0xB5/0xB6/0xB7 |

**命令说明**:
- 该命令用于模组向 MCU 下发管理类配置参数
- **当前版本中，CMD=0x09 仅用于状态记忆设置**
- MODE 固定为 0x02（开关模式），因为状态记忆仅适用于开关设备
- VALUE 直接表示状态记忆配置值：
  - 0x00：上电关（设备下次上电时默认关闭）
  - 0x01：上电开（设备下次上电时默认开启）
  - 0x02：上电保持（设备下次上电时保持断电前状态）

**示例 1 - 上电关设置**:
```
模组下发（模组→MCU）: AA 09 02 00 B5
MCU 反馈（MCU→模组）: AA 0F 02 00 BB
```
- CRC 计算：0xAA + 0x09 + 0x02 + 0x00 = 0xB5

**示例 2 - 上电开设置**:
```
模组下发（模组→MCU）: AA 09 02 01 B6
MCU 反馈（MCU→模组）: AA 0F 02 01 BC
```
- CRC 计算：0xAA + 0x09 + 0x02 + 0x01 = 0xB6

**示例 3 - 上电保持设置**:
```
模组下发（模组→MCU）: AA 09 02 02 B7
MCU 反馈（MCU→模组）: AA 0F 02 02 BD
```
- CRC 计算：0xAA + 0x09 + 0x02 + 0x02 = 0xB7

**处理流程**:
1. 模组通过 UART 发送 5 字节 HEX 帧：`AA 09 02 VALUE CRC`
2. MCU 的 `hex_protocol_receive_handler()` 解析帧
3. 验证 CRC 校验
4. 提取 VALUE 值（状态记忆配置）
5. 保存状态记忆配置到非易失性存储器
6. MCU 返回 ACK：`AA 0F 02 VALUE CRC`

**交互规则**:
1. **超时机制**：模组下发 CMD=0x09 后，MCU 必须在 200~500ms 内返回 CMD=0x0F
2. **重发机制**：如果超时未收到反馈，模组将重发，最大重发次数 3 次
3. **配置性质**：状态记忆设置为配置类参数，决定设备下一次上电后的默认输出状态
4. **不影响当前状态**：CMD=0x09 仅配置参数，不应改变当前 IO 输出状态

**注意事项**:
- 管理命令 (0x09) 为**单向命令**（模组→MCU）
- **仅适用于开关模式**（MODE=0x02），不适用于电机模式
- MCU 收到后必须返回 ACK 确认（CMD=0x0F）
- 当前版本 CMD=0x09 仅用于状态记忆设置，后续如需扩展其他管理类参数，将新增独立命令码

**代码实现参考**:
```c
// Hex 协议发送示例（drv_uart_protocol.c）
void drv_send_cmd9_push_hex(void)
{
    uint8_t frame[5];
    frame[0] = 0xAA;          // 起始符
    frame[1] = 0x09;          // 命令码
    frame[2] = 0x02;          // MODE: 开关模式（固定）
    frame[3] = uart2.lastSentMemValue;  // VALUE: 设置值（0x00/0x01/0x02）
    frame[4] = (frame[0] + frame[1] + frame[2] + frame[3]) & 0xFF;  // CRC
    liot_uart_write(LIOT_UART_PORT_2, frame, 5);
}

// 使用示例
uart2.lastSentMemValue = 0x00; drv_send_cmd9_push_hex();  // 上电关设置
uart2.lastSentMemValue = 0x01; drv_send_cmd9_push_hex();  // 上电开设置
uart2.lastSentMemValue = 0x02; drv_send_cmd9_push_hex();  // 上电保持设置
```

### 4.5 Hex 电机控制命令示例（0x01）

**说明**: 根据《UART 通信协议_HEX_V2.2.pdf》第 3.1 节定义

**帧格式**（5 字节）:
```
AA 01 MODE STATUS CRC
```

| 字段 | 长度 | 说明 | 示例值 |
|------|------|------|--------|
| 起始符 | 1 字节 | 固定为 0xAA | 0xAA |
| 命令码 | 1 字节 | 0x01（基本控制命令） | 0x01 |
| MODE | 1 字节 | 工作模式 | 0x00(HOLD)/0x01(JOG) |
| STATUS | 1 字节 | 电机状态 | 0x01(停止)/0x02(正转)/0x03(反转) |
| CRC | 1 字节 | 前 4 字节累加取低 8 位 | 计算值 |

**MODE 定义**:
| MODE 值 | 模式名称 | 说明 |
|---------|----------|------|
| 0x00 | HOLD | 联动（自锁）模式 |
| 0x01 | JOG | 点动模式 |

**STATUS 定义**:
| STATUS 值 | 状态名称 | 说明 |
|-----------|----------|------|
| 0x01 | STOP | 停止 |
| 0x02 | FORWARD | 正转 |
| 0x03 | REVERSE | 反转 |

**状态映射表**:
| motor.currentState | MODE | STATUS | 说明 |
|-------------------|------|--------|------|
| MODE_FORWARD_JOG | 0x01 | 0x02 | 点动正转 |
| MODE_REVERSE_JOG | 0x01 | 0x03 | 点动反转 |
| MODE_IDLE_JOG | 0x01 | 0x01 | 点动停止 |
| MODE_FORWARD_HOLD | 0x00 | 0x02 | 联动正转 |
| MODE_REVERSE_HOLD | 0x00 | 0x03 | 联动反转 |
| MODE_IDLE_HOLD | 0x00 | 0x01 | 联动停止 |

**示例 1 - 联动正转**:
```
模组下发（模组→MCU）: AA 01 00 02 0B
MCU 反馈（MCU→模组）: AA 0F 00 02 XX
```
- CRC 计算：0xAA + 0x01 + 0x00 + 0x02 = 0xAD，取低 8 位 0xAD

**示例 2 - 点动反转**:
```
模组下发（模组→MCU）: AA 01 01 03 0F
MCU 反馈（MCU→模组）: AA 0F 01 03 XX
```
- CRC 计算：0xAA + 0x01 + 0x01 + 0x03 = 0xAF，取低 8 位 0xAF

**处理流程**:
1. 模组通过 UART 发送 5 字节 HEX 帧：`AA 01 MODE STATUS CRC`
2. MCU 的 `hex_protocol_receive_handler()` 解析帧
3. 验证 CRC 校验
4. 提取 MODE 和 STATUS 值
5. 执行电机控制命令
6. MCU 返回 ACK：`AA 0F MODE STATUS CRC`

**交互规则**:
1. **超时机制**：模组下发 CMD=0x01 后，MCU 必须在 200~500ms 内返回 CMD=0x0F
2. **重发机制**：如果超时未收到反馈，模组将重发，最大重发次数 3 次
3. **反馈验证**：反馈的 MODE 和 STATUS 必须与发送的一致

**代码实现参考**:
```c
// Hex 协议发送示例（drv_uart_protocol.c）
void drv_send_cmd7_push_hex(void)
{
    uint8_t frame[5];
    uint8_t mode, status;
    
    // 1. 根据 motor.currentState 解析 MODE 和 STATUS
    // 2. 保存到 uart2.lastSentMotorMode 和 uart2.lastSentMotorStatus
    // 3. 构建帧并发送：AA 01 MODE STATUS CRC
}

// 使用示例
drv_send_cmd7_push_hex();  // 根据 motor.currentState 自动发送
```

## 5. 导出的公共函数

以下函数可在 `drv_hex_protocol.c` 中调用：

### 5.1 电机控制相关
```c
void drv_handle_cmd_7(const char *data, const char *msgid, int send_ack);
void drv_handle_cmd_8(const char *data, const char *msgid, int send_ack);
void update_motor_io_state(void);
```

### 5.2 开关控制相关
```c
void drv_handle_cmd_5(const char *data, const char *msgid, int send_ack);
```

### 5.3 故障处理相关
```c
const char* drv_get_fault_name(device_fault_t fault);
const char* hex_get_fault_name(uint8_t fault);
device_fault_t device_fault_from_char(char c);
void drv_cmd5_update_alarm(device_fault_t fault);
```

### 5.4 电压电流解析相关
```c
int drv_get_voltage_int(const char *v);
int drv_get_current_int(const char *a);
```

### 5.5 Hex 协议辅助函数
```c
uint8_t hex_protocol_calculate_crc(const uint8_t *data, uint8_t length);
int hex_protocol_verify(const uint8_t *buf, uint8_t len);
int hex_to_ascii_convert(hex_protocol_frame_t *hex_frame, char *ascii_data);
const char* hex_get_cmd_name(uint8_t cmd);
const char* hex_get_mode_name(uint8_t mode);
const char* hex_get_status_name(uint8_t mode, uint8_t status);
```

## 6. 测试验证

### 6.1 单元测试示例

```c
void test_hex_protocol(void)
{
    uint8_t test_basic[] = {0xAA, 0x01, 0x00, 0x02, 0x03};
    uint8_t test_extended_motor[] = {0xAA, 0x02, 0x00, 0x02, 0x01, 0x7C, 0x00, 0x24, 0x00, 0xA6};
    uint8_t test_extended_switch[] = {0xAA, 0x02, 0x02, 0x03, 0x01, 0x7C, 0x00, 0x24, 0x00, 0xA9};
    
    /* 测试基本命令（联动模式正转） */
    printf("Testing basic command (HOLD FORWARD)...\n");
    hex_protocol_receive_handler(test_basic, 5);
    
    /* 测试扩展命令（电机模式） */
    printf("Testing extended command (motor mode)...\n");
    hex_protocol_receive_handler(test_extended_motor, 10);
    
    /* 测试扩展命令（开关模式） */
    printf("Testing extended command (switch mode)...\n");
    hex_protocol_receive_handler(test_extended_switch, 10);
}
```

### 6.2 日志输出示例

**电机模式日志:**
```
[HEX] CRC check passed: calc=0xA6, recv=0xA6
[HEX] Parse success: CMD=2 (EXTENDED), MODE=0 (HOLD), STATUS=2 (FORWARD)
[HEX] Motor mode convert to ASCII: LFV380.0A036.0NN0
[HEX] Motor mode: calling CMD8 handler
[DRV] CMD8 data=LFV380.0A036.0NN0 msgid=00
```

**开关模式日志:**
```
[HEX] CRC check passed: calc=0xA9, recv=0xA9
[HEX] Parse success: CMD=2 (EXTENDED), MODE=2 (SWITCH), STATUS=3
[HEX] Switch mode IO status: 0011
[HEX] Switch mode convert to ASCII: V380.0A036.00011NN0
[HEX] Switch mode: calling CMD5 handler
[DRV] CMD5 data=V380.0A036.00011NN0 msgid=00
```

## 7. 注意事项

### 7.1 CRC 计算
- **Hex 协议 CRC**: 所有字节求和，取低 8 位
  - 公式：`CRC = (CMD + SUM(DATA)) & 0xFF`
  - 示例：`AA 01 00 02` → CRC = (0x01+0x00+0x02) & 0xFF = 0x03
- **ASCII 协议 CRC**: ASCII 字符求和，取后两位

### 7.2 数据格式
- **Hex 协议**: 二进制格式，电压/电流为 16 位整数
  - 电压：高字节在前，单位 0.1V (0x017C = 380V)
  - 电流：高字节在前，单位 0.1A (0x0024 = 3.6A)
- **ASCII 协议**: 字符串格式，电压/电流为"XXX.X"格式

### 7.3 消息 ID
- Hex 协议没有消息 ID 字段
- 转换后使用默认值 "00"

### 7.4 开关模式 IO 控制
- **IO 状态字段**: 1 字节，按 bit 控制 4 路输出
  - bit0: IO1 (最低位)
  - bit1: IO2
  - bit2: IO3
  - bit3: IO4 (最高位)
- **状态转换**: 0x0F → "1111" (全开), 0x03 → "0011" (IO1+IO2 开)
- **基本命令限制**: 开关模式不支持基本命令 (0x01)，必须使用扩展命令 (0x02)

### 7.5 ACK 响应
- **基本命令**: 发送 Hex ACK (0x0F) 响应
  - 格式：`AA 0F MODE STATUS CRC`
- **扩展命令**: 暂不发送 ACK 响应
- **管理命令 (0x09)**: MCU 收到后需返回 ACK
  - 格式：`AA 0F 00 00 CRC`（通用确认）

### 7.6 协议版本
- 当前实现基于 **UART 通信协议_HEX_V2.2**
- 与 V2.1 相比，V2.2 新增了开关模式 (SWITCH) 支持

## 8. 故障码映射表

### 8.1 Hex 故障码定义

| Hex 值 | 故障代码 | ASCII 映射 | 说明 |
|--------|----------|-----------|------|
| 0x00 | 无故障 | '0' | 正常运行 |
| 0x04 | 定时到 | '1' | 定时时间到达（扩展） |
| 0x14 | E-20 | 'A' | 缺相 A |
| 0x15 | E-21 | 'B' | 缺相 B |
| 0x16 | E-22 | 'C' | 缺相 C |
| 0x17 | E-23 | '6' | 空载 |
| 0x18 | 漏电 | '8' | 漏电保护（扩展） |
| 0x19 | E-25 | 'D' | 短路 |
| 0x1A | E-26 | '7' | 过载 |
| 0x1E | 过压 | '3' | 过压保护（扩展） |
| 0x1F | 欠压 | '2' | 欠压保护（扩展） |

### 8.2 故障测试示例

**电机模式故障上报:**
```
AA 02 00 02 01 7C 00 24 14 B9  // 正转 - 缺相 A 故障
AA 02 00 02 01 7C 00 24 17 BC  // 正转 - 空载故障
AA 02 00 02 01 7C 00 24 1A BF  // 正转 - 过载故障
```

**开关模式故障上报:**
```
AA 02 02 01 01 7C 00 24 14 C9  // 只开 IO1 - 缺相 A 故障
AA 02 02 03 01 7C 00 24 17 CA  // 开 IO1+IO2 - 空载故障
AA 02 02 0F 01 7C 00 24 1A D6  // 全开 - 过载故障
```

## 9. 修改记录

- 2026-03-18: 创建 Hex 协议解析模块
- 2026-03-18: 完成协议转换逻辑
- 2026-03-18: 导出公共函数供外部调用
- 2026-03-28: 基于 V2.2 协议更新文档
  - 新增开关模式 (SWITCH) 支持
  - 更新故障码映射表
  - 补充 IO4 控制说明
  - 添加 ACK 响应机制说明
- 2026-03-28: 补充 0x09 管理命令文档（第一版）
  - 添加管理命令 (0x09) 定义和说明
  - 补充管理命令示例和 CRC 计算
  - 更新协议转换表和快速参考
- 2026-03-28: 修正 0x09 管理命令格式（根据 PDF V2.2）
  - 修正帧格式为 5 字节（AA 09 MODE VALUE CRC）
  - 更新示例为上电关/开/保持设置
  - 补充交互规则和代码实现参考
- 2026-03-29: 新增 CMD7 Hex 协议支持
  - 新增 drv_send_cmd7_push_hex() 函数
  - 新增 drv_send_cmd7_push_ack_check_hex() 函数
  - 更新 task_uart.h 结构体（新增 lastSentMotorMode/Status）
  - 更新 drv_uart_protocol.h 头文件声明
  - 修改 drv_cloud_common.c 调用代码
  - 修改 drv_hex_protocol.c 反馈处理
  - 补充 CMD7 的 HEX 协议文档说明

## 10. 相关文件

- `drv_hex_protocol.h`: Hex 协议头文件
- `drv_hex_protocol.c`: Hex 协议实现文件
- `drv_uart_protocol.h`: ASCII UART 协议头文件
- `drv_uart_protocol.c`: ASCII UART 协议实现文件
- `UART 通信协议_HEX_V2.2.pdf`: Hex 协议规范文档（最新版本）
- `HEX 协议测试指令大全.txt`: 完整的测试指令集

## 11. 快速参考

### 11.1 常用电机模式指令（带 CRC）

```
AA 01 00 02 04    // 联动模式 - 正转
AA 01 00 03 05    // 联动模式 - 反转
AA 01 01 02 05    // 点动模式 - 正转
AA 02 00 02 01 7C 00 24 00 A6  // 联动正转上报 (380V, 3.6A, 无故障)
```

### 11.2 常用开关模式指令（带 CRC）

```
AA 02 02 01 01 7C 00 24 00 A8  // 只开 IO1 (380V, 3.6A, 无故障)
AA 02 02 03 01 7C 00 24 00 A9  // 开 IO1+IO2
AA 02 02 0F 01 7C 00 24 00 B4  // 全开 (IO1-IO4)
AA 02 02 00 01 7C 00 24 00 A7  // 全关
```

### 11.3 常用管理命令指令（带 CRC）

```
AA 09 02 00 B5  // 上电关设置（模组→MCU）
AA 09 02 01 B6  // 上电开设置（模组→MCU）
AA 09 02 02 B7  // 上电保持设置（模组→MCU）
```

**说明**: 
- 管理命令格式：AA + 09 + MODE(1 字节) + VALUE(1 字节) + CRC（5 字节）
- MODE 字段：固定为 0x02（开关模式）
- VALUE 字段：1 字节（0x00:上电关，0x01:上电开，0x02:上电保持）
- 模组发送后 MCU 需返回 ACK：`AA 0F 02 VALUE CRC`
- 当前版本 CMD=0x09 仅用于状态记忆设置

### 11.4 IO 状态速查表

| Hex 值 | 二进制 | IO 状态 | ASCII 转换 |
|--------|--------|---------|-----------|
| 0x00 | 0000 | 全关 | "0000" |
| 0x01 | 0001 | IO1 开 | "0001" |
| 0x02 | 0010 | IO2 开 | "0010" |
| 0x03 | 0011 | IO1+IO2 开 | "0011" |
| 0x07 | 0111 | IO1+IO2+IO3 开 | "0111" |
| 0x0F | 1111 | 全开 | "1111" |
