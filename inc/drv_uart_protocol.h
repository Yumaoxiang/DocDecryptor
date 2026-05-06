#ifndef __UART_PROTOCOL_DRV_H__
#define __UART_PROTOCOL_DRV_H__

#include <stdint.h>
#include "hal_project.h"



/* UART 网络状态枚举与结构（用于 CMD='3' 状态映射），名称包含 'uart' 前缀以便于识别*/
typedef enum {
    uart_net_status_PWR = 0,
    uart_net_status_SIM,
    uart_net_status_NET,
    uart_net_status_CLD,
    uart_net_status_SUC,
    uart_net_status_FAL,
} uart_net_status_e;

typedef struct {
    uart_net_status_e status;
    char code[4]; /* 三字符状态码 + '\0' */
} uart_net_status_t;


typedef enum {
    enIO_OUT_OFF=0,                         //IO引脚输出状态，通用，关闭
    enIO_OUT_ON=1,                          //IO引脚输出状态，通用，开启
} e_OUT_STATUS;

typedef struct {
    e_OUT_STATUS     pre_status;
    e_OUT_STATUS     now_status;
} OUT_IO;

typedef struct {
    // 数组元素含义（索引0-3分别对应IO4-IO1）：
    OUT_IO      out[4];             // 系统相关参数，改为数组形式，支持索引访问
} IOOutputStatus;
extern IOOutputStatus      g_io;            // IO输出状态，g代表global，即全局变量


// /**
//  * @brief CMD='9' 管理设置命令 - 设置命令字段 (3字节 ASCII)
//  */
// typedef enum {
//     MGMT_CMD_MEM = 0,  // "MEM": 设置上电状态记忆
//     MGMT_CMD_RST = 1,  // "RST": 触发模组复位/重启
//     MGMT_CMD_GET = 2,  // "GET": 查询模组参数 (预留)
//     MGMT_CMD_SET = 3,  // "SET": 配置上报周期等参数 (预留)
//     MGMT_CMD_CLR = 4,  // "CLR": 清除配置/初始化 (预留) 
//     MGMT_CMD_UNKNOWN = 0xFF
// } ManagementCmdType;


typedef enum
{
    DEV_FAULT_NONE          = '0',  // 无故障
    DEV_FAULT_TIMER         = '1',  // 定时到
    DEV_FAULT_UNDERVOLT     = '2',  // 欠压
    DEV_FAULT_OVERVOLT      = '3',  // 过压
    DEV_FAULT_UNDERCURRENT  = '4',  // 欠流
    DEV_FAULT_OVERCURRENT   = '5',  // 过流
    DEV_FAULT_NOLOAD        = '6',  // 空载
    DEV_FAULT_OVERLOAD      = '7',  // 过载
    DEV_FAULT_LEAKAGE       = '8',  // 漏电
    DEV_FAULT_PHASE_LOSS    = '9',  // 缺相
    DEV_FAULT_PHASE_A       = 'A',  // 缺相A
    DEV_FAULT_PHASE_B       = 'B',  // 缺相B
    DEV_FAULT_PHASE_C       = 'C',  // 缺相C
    DEV_FAULT_SHORT_CIRCUIT = 'D',  // 短路

    DEV_FAULT_INVALID       = 0xFF  // 非法值
} device_fault_t;


typedef struct
{
    device_fault_t fault;           // 设备故障码
    const char     alarm[4];        // 3字节报警字符串 + '\0'
} fault_alarm_map_t;



void drv_send_cmd3_push_net_status(void);       // 根据当前网络状态发送 CMD='3' 状态帧，包含状态映射与 msgID 处理
void drv_send_cmd3_push(char *network_status);  // 发送 CMD='3' 状态帧，参数为三字符网络状态码（如 "PWR"），包含 msgID 处理

void drv_send_cmd4_push_str();                      // 发送 CMD='4' 状态帧，包含 IO 状态映射与 msgID 处理
int drv_send_cmd4_push_ack_check_str();             // 发送 CMD='4' 后检测是否收到合法 ACK，返回 1 表示收到合法 ACK，0 表示未收到或 ACK 不合法
void drv_send_cmd4_push_hex();                      // 发送 CMD='4' 状态帧，包含 IO 状态映射与 msgID 处理
int drv_send_cmd4_push_ack_check_hex();             // 发送 CMD='4' 后检测是否收到合法 ACK，包含 IO 状态映射与 msgID 处理

void drv_send_cmd7_push_str(void);                  // 发送 CMD='7' 状态帧（ASCII 协议版本），包含电机控制状态映射与 msgID 处理
void drv_send_cmd7_push_hex(void);                  // 发送 CMD=0x01 基本命令（Hex 协议版本），包含电机控制状态映射
int drv_send_cmd7_push_ack_check(void);             // 发送 CMD='7' 后检测是否收到合法 ACK（ASCII 协议），返回 1 表示收到合法 ACK，0 表示未收到或 ACK 不合法
int drv_send_cmd7_push_ack_check_hex(void);         // 发送 CMD=0x01 后检测是否收到合法 ACK（Hex 协议），返回 1 表示收到合法 ACK，0 表示未收到或 ACK 不合法

void drv_send_cmd9_push_str(const char *func_str);  // 发送 CMD='9'管理设置命令（ASCII 协议版本），参数为管理命令字符串（如"MEM"）
void drv_send_cmd9_push_hex(void);                  // 发送 CMD=0x09 管理设置命令（Hex 协议版本），使用 uart2.lastSentMemValue 作为设置值
int drv_send_cmd9_push_ack_check();                 // 发送 CMD='9'后检测是否收到合法 ACK（ASCII 协议），返回 1 表示收到合法 ACK，0 表示未收到或 ACK 不合法
int drv_send_cmd9_push_ack_check_hex(void);         // 发送 CMD=0x09 后检测是否收到合法 ACK（Hex 协议），返回 1 表示收到合法 ACK，0 表示未收到或 ACK 不合法



/* 基于缓冲区和长度进行校验（返回1合法，0非法） */
int uart_protocol_check_preamble(const unsigned char *buf, unsigned int len);
int uart_protocol_check_protocol(const unsigned char *buf, unsigned int len);
void uart_protocol_calculate_crc(const unsigned char *str, int length, unsigned char* result);
e_OUT_STATUS map_data_to_io_status(char data_char);
void update_io_state_from_g_io();               // 根据 g_io 中的状态更新实际 IO 输出（开关产品或电机产品）
const char* drv_get_fault_name(device_fault_t fault);   // 根据故障码获取故障名称字符串 (用于日志打印)

/*CMD 调度到具体处理函数*/
void uart_receive_execute_command(void);
void uart_protocol_parse_fields(const unsigned char *buf, int len, char *cmd_out, char *data_out, char *msgid_out); // 解析字段（提取 CMD / DATA / msgID），data_out 长度建议 256，msgid_out 长度至少 3

void drv_handle_cmd_3(const char *data, const char *msgid);

void drv_handle_cmd_4(const char *data, const char *msgid);
int drv_cmd4_check_platform_protect_and_ack(const char *msgid); // 检查是否处于平台命令2秒保护期，若是则自动ACK并返回1，否则返回0继续执行
int drv_cmd4_parse_and_update_io(const char *data);             // 解析CMD4的data字段并更新g_io状态，返回1表示IO状态发生变化，0表示无变化
void drv_cmd4_handle_io_changed(void);                          // IO状态变化后发送平台消息并更新IO输出
void drv_cmd4_send_ack(const char *msgid);                      // 发送CMD4 ACK，包含msgID处理

void drv_handle_cmd_5(const char *data, const char *msgid, int send_ack);  /* 增加 send_ack 参数 */
int drv_get_voltage_int(const char *v);                         // 从电压字符串（如 "220.0"）中提取整数部分，返回220
int drv_get_current_int(const char *a);                         // 从电流字符串（如 "030"）中提取整数部分，返回30
int drv_cmd5_parse_voltage(const char *data);                   // 从CMD5的data字段中解析电压值，返回是否发生变化（1表示变化，0表示无变化）
int drv_cmd5_parse_current(const char *data);                   // 从CMD5的data字段中解析电流值，返回是否发生变化（1表示变化，0表示无变化）
int drv_cmd5_parse_switch(const char *data);                    // 从CMD5的data字段中解析开关状态，返回是否发生变化（1表示变化，0表示无变化）
int drv_cmd5_parse_fault(const char *data);                     // 从CMD5的data字段中解析故障状态，返回是否发生变化（1表示变化，0表示无变化）
void drv_cmd5_update_alarm(device_fault_t fault);               // 根据设备故障码更新 cat1.param.alarm 字段

void drv_handle_cmd_7(const char *data, const char *msgid, int send_ack);  // send_ack=1 发送 ACK，send_ack=0 不发送
int drv_cmd7_check_platform_protect_and_ack(const char *msgid);  // 检查是否处于平台命令 2 秒保护期内，若是则自动 ACK 并返回 1，否则返回 0
int drv_cmd7_parse_and_validate(const char *data);              // 解析 CMD7 的 data 字段并验证命令合法性（2 字符组合：模式 + 状态），返回 1 表示合法，0 表示非法
int drv_cmd7_execute_command(const char *data);                 // 执行 CMD7 的电机控制命令，返回状态是否变化（1 表示变化，0 表示无变化）
void drv_cmd7_handle_motor_changed(void);                       // 处理电机状态变化后的逻辑（通知平台 + 更新 IO 输出）
void drv_cmd7_send_ack(char result, const char *msgid);         // 发送 CMD7 ACK，result='0'表示成功，'1'表示失败
void update_motor_io_state(void);                               // 根据 motor.currentState 更新电机控制 IO 输出

void drv_handle_cmd_8(const char *data, const char *msgid, int send_ack);  // send_ack=1 发送 ACK，send_ack=0 不发送
int drv_cmd8_check_platform_protect_and_ack(const char *msgid);  // 检查是否处于平台命令 2 秒保护期内，若是则自动 ACK 并返回 1，否则返回 0
int drv_cmd8_parse_and_validate(const char *data);              // 解析 CMD8 的 data 字段并验证命令合法性（2 字符组合：模式 + 状态），返回 1 表示合法，0 表示非法
int drv_cmd8_parse_voltage(const char *data);                   // 从 CMD8 的 data 字段中解析电压值，返回是否发生变化（1 表示变化，0 表示无变化）
int drv_cmd8_parse_current(const char *data);                   // 从 CMD8 的 data 字段中解析电流值，返回是否发生变化（1 表示变化，0 表示无变化）
int drv_cmd8_parse_fault(const char *data);                     // 从 CMD8 的 data 字段中解析故障状态，返回是否发生变化（1 表示变化，0 表示无变化）
int drv_cmd8_execute_command(const char *data);                 // 执行 CMD8 的电机控制命令，返回状态是否变化（1 表示变化，0 表示无变化）
void drv_cmd8_handle_changed(void);                             // 处理 CMD8 状态变化后的逻辑（通知平台 + 更新 IO 输出）
void drv_cmd8_send_ack(char result, const char *msgid);         // 发送 CMD8 ACK，result='0'表示成功，'1'表示失败

void drv_handle_cmd_9(const char *data, const char *msgid);

void drv_handle_cmd_F(const char *data, const char *msgid);
void uart_protocol_build_ack(char src_cmd, char result, const char *msgid, unsigned char *out_buf, int *out_len);   // 组包 ACK：生成到 out_buf，out_len 返回字节数（out_buf 建议 >= 32）

/* 故障处理相关（新增导出） */
const char* drv_get_fault_name(device_fault_t fault);                   // 获取故障名称字符串（供外部模块调用）
device_fault_t device_fault_from_char(char c);                          // 字符转换为故障码（供外部模块调用）



#endif
