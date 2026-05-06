#ifndef __DRV_HEX_PROTOCOL_H__
#define __DRV_HEX_PROTOCOL_H__

#include <stdint.h>
#include "hal_project.h"

/***************************************************************************************
 * 宏定义
 **************************************************************************************/

/* Hex 协议帧最小/最大长度 */
#define HEX_PROTOCOL_MIN_FRAME_LEN    5       // 基本命令：AA+CMD+MODE+STATUS+CRC
#define HEX_PROTOCOL_MAX_FRAME_LEN    10      // 扩展命令：AA+CMD+MODE+STATUS+VOLT+CURR+FAULT+CRC

/* 命令码定义 */
#define HEX_CMD_BASIC                 0x01    // 基本控制命令
#define HEX_CMD_EXTENDED              0x02    // 扩展状态上报
#define HEX_CMD_FEEDBACK              0x0F    // 反馈命令 (MCU→模组)

/* 工作模式定义 */
#define HEX_MODE_HOLD                 0x00    // 联动 (自锁) 模式 - 电机产品
#define HEX_MODE_JOG                  0x01    // 点动模式 - 电机产品
#define HEX_MODE_SWITCH               0x02    // 开关模式 - 开关产品（新增）

/* 开关状态定义 */
#define HEX_STATUS_STOP               0x01    // 停止
#define HEX_STATUS_FORWARD            0x02    // 正转
#define HEX_STATUS_REVERSE            0x03    // 反转

/* 故障码定义 */
#define HEX_FAULT_NONE                0x00    // 无故障
#define HEX_FAULT_TIMING_UP           0x04    // 定时到
#define HEX_FAULT_PHASE_A             0x14    // E-20 缺相 A
#define HEX_FAULT_PHASE_B             0x15    // E-21 缺相 B
#define HEX_FAULT_PHASE_C             0x16    // E-22 缺相 C
#define HEX_FAULT_NOLOAD              0x17    // E-23 空载
#define HEX_FAULT_LEAKAGE             0x18    // 漏电
#define HEX_FAULT_SHORT_CIRCUIT       0x19    // E-25 短路
#define HEX_FAULT_OVERLOAD            0x1A    // E-26 过载
#define HEX_FAULT_OVER_VOLTAGE        0x1E    // 过压
#define HEX_FAULT_UNDER_VOLTAGE       0x1F    // 欠压

/***************************************************************************************
 * 数据结构定义
 **************************************************************************************/

/* Hex 协议帧结构 */
typedef struct {
    uint8_t start;      // 起始符 0xAA
    uint8_t cmd;        // 命令码 0x01/0x02/0x09/0x0F
    uint8_t mode;       // 工作模式 0x00/0x01/0x02
    uint8_t status;     // 开关状态 0x01/0x02/0x03
    uint16_t voltage;   // 电压 (0x017C=380V)
    uint16_t current;   // 电流 (0x0024=3.6A)
    uint8_t fault;      // 故障码 0x00-0x1F
    uint8_t crc;        // CRC 校验和
} hex_protocol_frame_t;

/***************************************************************************************
 * 函数声明
 **************************************************************************************/

/* Hex 协议 CRC 校验 */
uint8_t hex_protocol_calculate_crc(const uint8_t *data, uint8_t length);

/* Hex 协议帧解析 */
int hex_protocol_parse(const uint8_t *buf, uint8_t len, hex_protocol_frame_t *frame);

/* Hex 协议验证 */
int hex_protocol_verify(const uint8_t *buf, uint8_t len);

/* Hex 到 ASCII 协议转换 */
int hex_to_ascii_convert(hex_protocol_frame_t *hex_frame, char *ascii_data);

/* Hex 协议主处理函数 */
void hex_protocol_receive_handler(const uint8_t *buf, uint8_t len);

/* Hex 反馈命令处理 */
void hex_handle_feedback_command(hex_protocol_frame_t *frame);

/* 判断是否为 Hex 协议 */
int is_hex_protocol(const uint8_t *buf, uint8_t len);

/* Hex 数据打印 */
void hex_protocol_print_buffer(const uint8_t *data, uint8_t len);
void hex_protocol_print_data(const uint8_t *data, uint8_t len);

/* Hex 协议字符串转换 */
const char* hex_get_cmd_name(uint8_t cmd);                      /* 获取命令名称 */
const char* hex_get_mode_name(uint8_t mode);                    /* 获取模式名称 */
const char* hex_get_status_name(uint8_t mode, uint8_t status);  /* 获取状态名称 */
const char* hex_get_fault_name(uint8_t fault);                  /* 获取故障名称 */

#endif /* __DRV_HEX_PROTOCOL_H__ */
