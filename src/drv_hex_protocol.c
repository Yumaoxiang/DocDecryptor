/***************************************************************************
 * 文件名：drv_hex_protocol.c
 * 描述：Hex 格式 UART 协议解析与处理实现
 * 版本：1.0
 * 创建日期：2026-03-18
 * 修改记录：
 *   - 2026-03-18: 初始版本
 *   - 2026-03-18: 添加编译时协议格式配置支持
 * 
 * 使用说明：
 *   - 本文件在 UART_PROTOCOL_FORMAT_HEX=1 时编译使用
 *   - 在 hal_project.h 中配置 UART_PROTOCOL_FORMAT_HEX 和 UART_PROTOCOL_FORMAT_ASCII
 *   - Hex 协议和 ASCII 协议二选一，不能同时启用
 *   - 运行时根据编译时配置自动选择协议处理方式
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "liot_os.h"
#include "liot_type.h"
#include "liot_uart.h"
#include "task_uart.h"
#include "hal_project.h"
#include "drv_hex_protocol.h"
#include "drv_uart_protocol.h"

/***************************************************************************************
 * 全局变量
 **************************************************************************************/


/***************************************************************************************
 * 函数实现
 **************************************************************************************/

/******************************************************************************************************
 * 名    称：hex_protocol_print_data
 * 功    能：以 Hex 格式打印数据数组
 * 入口参数：data - 数据缓冲区，len - 数据长度
 * 出口参数：无
 * 示    例：对于数组 {0xAA, 0x01, 0x02}，打印 "AA 01 02"
 * 说    明：循环打印每个字节，格式为 2 位 16 进制，大写，空格分隔
 *****************************************************************************************************/
void hex_protocol_print_data(const uint8_t *data, uint8_t len)
{
    if (!data || len == 0) {
        return;
    }
    
    /* 循环打印每个字节 */
    for (uint8_t i = 0; i < len; i++) {
        liot_trace("%X", data[i]);
    }
}

/******************************************************************************************************
 * 名    称：hex_protocol_print_buffer
 * 功    能：以 Hex 格式打印数据数组（单行格式）
 * 入口参数：data - 数据缓冲区，len - 数据长度
 * 出口参数：无
 * 示    例：对于数组 {0xAA, 0x01, 0x02}，打印 "AA 01 02"
 * 说    明：将所有字节打印在一行，空格分隔，便于日志查看
 *         每个字节占 2 位，不足补 0，例如 0x01 打印为 "01"
 *****************************************************************************************************/
void hex_protocol_print_buffer(const uint8_t *data, uint8_t len)
{
    char hex_str[64];  /* 足够存储 10 字节的 hex 字符串 + 前缀 */
    char temp[8];
    
    if (!data || len == 0) {
        liot_trace("[HEX] Empty buffer");
        return;
    }
    
    /* 构建前缀 */
    strcpy(hex_str, "[HEX] ");
    
    /* 限制最大打印长度，避免缓冲区溢出 */
    uint8_t print_len = (len > 10) ? 10 : len;
    
    /* 循环构建 hex 字符串 */
    for (uint8_t i = 0; i < print_len; i++) {
        sprintf(temp, "%02X", data[i]);
        strcat(hex_str, temp);
        if (i < print_len - 1) {
            strcat(hex_str, " ");
        }
    }
    
    /* 如果超过 10 字节，添加提示 */
    if (len > 10) {
        strcat(hex_str, " ...");
    }
    
    /* 一次性打印 */
    liot_trace("%s", hex_str);
}

/******************************************************************************************************
 * 名    称：hex_get_cmd_name
 * 功    能：根据命令码获取命令名称字符串 (用于日志打印)
 * 入口参数：cmd - 命令码 (0x01/0x02/0x0F)
 * 出口参数：返回对应的命令名称字符串
 * 示    例：0x01 → "BASIC", 0x02 → "EXTENDED", 0x0F → "ACK"
 *****************************************************************************************************/
const char* hex_get_cmd_name(uint8_t cmd)
{
    switch (cmd) {
        case HEX_CMD_BASIC:
            return "BASIC";
        case HEX_CMD_EXTENDED:
            return "EXTENDED";
        case HEX_CMD_FEEDBACK:
            return "FEEDBACK";
        default:
            return "UNKNOWN";
    }
}

/******************************************************************************************************
 * 名    称：hex_get_mode_name
 * 功    能：根据工作模式获取模式名称字符串 (用于日志打印)
 * 入口参数：mode - 工作模式 (0x00/0x01)
 * 出口参数：返回对应的模式名称字符串
 * 示    例：0x00 → "HOLD", 0x01 → "JOG"
 *****************************************************************************************************/
const char* hex_get_mode_name(uint8_t mode)
{
    switch (mode) {
        case HEX_MODE_HOLD:
            return "HOLD";
        case HEX_MODE_JOG:
            return "JOG";
        case HEX_MODE_SWITCH:
            return "SWITCH";
        default:
            return "UNKNOWN";
    }
}

/******************************************************************************************************
 * 名    称：hex_get_status_name
 * 功    能：根据开关状态获取状态名称字符串 (用于日志打印)
 * 入口参数：status - 开关状态 (0x01/0x02/0x03)
 * 出口参数：返回对应的状态名称字符串
 * 示    例：0x01 → "STOP", 0x02 → "FORWARD", 0x03 → "REVERSE"
 *****************************************************************************************************/
/******************************************************************************************************
 * 名    称：hex_get_status_name
 * 功    能：获取开关状态的字符串名称
 * 入口参数：mode - 工作模式，status - 开关状态值
 * 出口参数：返回状态名称字符串
 * 说    明：
 *  - 电机模式 (HOLD/JOG): 返回 STOP/FORWARD/REVERSE
 *  - 开关模式 (SWITCH): 按 bit 解析，返回 IO 状态字符串 (例如 "IO1=ON,IO2=OFF")
 *****************************************************************************************************/
const char* hex_get_status_name(uint8_t mode, uint8_t status)
{
    static char io_status_str[32];  /* 静态缓冲区，用于开关模式 */
    
    /* 开关模式：按 bit 解析 status，同时包含 Mem 状态解析 */
    if (mode == HEX_MODE_SWITCH) {
        /* IO 状态：bit0-bit3 表示 IO1-IO4 的开关状态 */
        uint8_t io1 = (status & 0x01) ? 1 : 0;  /* bit0: IO1 */
        uint8_t io2 = (status & 0x02) ? 1 : 0;  /* bit1: IO2 */
        uint8_t io3 = (status & 0x04) ? 1 : 0;  /* bit2: IO3 */
        uint8_t io4 = (status & 0x08) ? 1 : 0;  /* bit3: IO4 */
        
        /* Mem 状态：bit0-bit1 也表示上电记忆状态 */
        uint8_t mem = status & 0x03;
        const char *mem_str;
        switch (mem) {
            case 0x00:
                mem_str = "Clos";
                break;
            case 0x01:
                mem_str = "Open";
                break;
            case 0x02:
                mem_str = "Prev";
                break;
            default:
                mem_str = "----";
                break;
        }
        
        /* 组合打印：IO状态 + Mem状态，用户自行匹配判断 */
        sprintf(io_status_str, "IO=%d%d%d%d,Mem=(%s)", io4, io3, io2, io1, mem_str);       
        return io_status_str;
    }
    
    /* 电机模式 (HOLD/JOG): 返回正反转状态 */
    switch (status) {
        case HEX_STATUS_STOP:
            return "STOP";
        case HEX_STATUS_FORWARD:
            return "FORWARD";
        case HEX_STATUS_REVERSE:
            return "REVERSE";
        default:
            return "UNKNOWN";
    }
}

/******************************************************************************************************
 * 名    称：hex_get_fault_name
 * 功    能：根据故障码获取故障名称字符串 (用于日志打印)
 * 入口参数：fault - 故障码
 * 出口参数：返回对应的故障名称字符串
 * 示    例：0x00 → "NONE", 0x14 → "PHASE_A", 0x17 → "NOLOAD"
 *****************************************************************************************************/
const char* hex_get_fault_name(uint8_t fault)
{
    switch (fault) {
        case HEX_FAULT_NONE:
            return "NONE";
        case HEX_FAULT_TIMING_UP:
            return "TIMING_UP";
        case HEX_FAULT_PHASE_A:
            return "PHASE_A";
        case HEX_FAULT_PHASE_B:
            return "PHASE_B";
        case HEX_FAULT_PHASE_C:
            return "PHASE_C";
        case HEX_FAULT_NOLOAD:
            return "NOLOAD";
        case HEX_FAULT_LEAKAGE:
            return "LEAKAGE";
        case HEX_FAULT_SHORT_CIRCUIT:
            return "SHORT_CIRCUIT";
        case HEX_FAULT_OVERLOAD:
            return "OVERLOAD";
        case HEX_FAULT_OVER_VOLTAGE:
            return "OVER_VOLTAGE";
        case HEX_FAULT_UNDER_VOLTAGE:
            return "UNDER_VOLTAGE";
        default:
            return "UNKNOWN";
    }
}

/***************************************************************************************
 * 内部函数声明
 **************************************************************************************/

static int hex_parse_basic_command(hex_protocol_frame_t *frame);
static int hex_parse_extended_command(hex_protocol_frame_t *frame);
static void hex_handle_basic_command(hex_protocol_frame_t *frame);
static void hex_handle_extended_command(hex_protocol_frame_t *frame);
// static void hex_send_basic_ack(uint8_t status);
// static void hex_send_extended_ack(void);

/***************************************************************************************
 * 函数实现
 **************************************************************************************/

/******************************************************************************************************
 * 名    称：hex_send_basic_ack
 * 功    能：发送 Hex 协议基本命令的 ACK 响应
 * 入口参数：status - 开关状态 (0x01/0x02/0x03)
 * 出口参数：无
 * 示    例：发送 AA 0F 00 02 XX (正转 ACK)
 * 说    明：5 字节 Hex 格式 ACK
 *****************************************************************************************************/
// static void hex_send_basic_ack(uint8_t status)
// {
//     uint8_t ack_buf[5];
    
//     ack_buf[0] = 0xAA;        /* 起始符 */
//     ack_buf[1] = 0x0F;        /* ACK 命令 */
//     ack_buf[2] = 0x00;        /* 预留 */
//     ack_buf[3] = status;      /* 状态 */
//     ack_buf[4] = hex_protocol_calculate_crc(ack_buf, 4);  /* CRC */
    
//     liot_trace("[HEX] Send basic ACK: ");
//     hex_protocol_print_buffer(ack_buf, 5);
    
//     liot_uart_write(LIOT_UART_PORT_2, ack_buf, 5);
// }

/******************************************************************************************************
 * 名    称：hex_send_extended_ack
 * 功    能：发送 Hex 协议扩展命令的 ACK 响应
 * 入口参数：无
 * 出口参数：无
 * 示    例：发送 AA 0F 00 02 XX
 * 说    明：5 字节 Hex 格式 ACK
 *****************************************************************************************************/
// static void hex_send_extended_ack(void)
// {
//     uint8_t ack_buf[5];
    
//     ack_buf[0] = 0xAA;        /* 起始符 */
//     ack_buf[1] = 0x0F;        /* ACK 命令 */
//     ack_buf[2] = 0x00;        /* 预留 */
//     ack_buf[3] = 0x00;        /* 预留 */
//     ack_buf[4] = hex_protocol_calculate_crc(ack_buf, 4);  /* CRC */
    
//     liot_trace("[HEX] Send extended ACK: ");
//     hex_protocol_print_buffer(ack_buf, 5);
    
//     liot_uart_write(LIOT_UART_PORT_2, ack_buf, 5);
// }

/******************************************************************************************************
 * 名    称：hex_protocol_calculate_crc
 * 功    能：Hex 协议 CRC 校验（所有字节求和，取低 8 位）
 * 入口参数：data - 数据缓冲区，length - 数据长度（不包含 CRC 字节）
 * 出口参数：返回计算得到的 CRC 值
 * 示    例：对于帧 AA 02 00 01 01 7C 00 24 00，返回 0x4C
 * 说    明：CRC 为前 N 位求和，仅取结果低 8 位
 *****************************************************************************************************/
uint8_t hex_protocol_calculate_crc(const uint8_t *data, uint8_t length)
{
    uint16_t sum = 0;
    
    if (!data || length == 0) {
        return 0;
    }
    
    /* 所有字节求和 */
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    
    /* 取低 8 位 */
    return (uint8_t)(sum & 0xFF);
}

/******************************************************************************************************
 * 名    称：hex_protocol_verify
 * 功    能：验证 Hex 协议帧的 CRC 校验
 * 入口参数：buf - 数据缓冲区，len - 数据长度
 * 出口参数：返回 1 表示 CRC 正确，返回 0 表示 CRC 错误
 * 说    明：检查整个帧的 CRC 是否正确
 *****************************************************************************************************/
int hex_protocol_verify(const uint8_t *buf, uint8_t len)
{
    uint8_t calculated_crc;
    uint8_t received_crc;
    
    if (!buf || len < HEX_PROTOCOL_MIN_FRAME_LEN) {
        return 0;
    }
    
    /* 计算 CRC（不包含最后一个 CRC 字节） */
    calculated_crc = hex_protocol_calculate_crc(buf, len - 1);
    
    /* 获取接收到的 CRC */
    received_crc = buf[len - 1];
    
    if (calculated_crc == received_crc) {
        liot_trace("[HEX] CRC check passed: calc=0x%X (%d), recv=0x%X (%d)", 
                   calculated_crc, calculated_crc, received_crc, received_crc);
        return 1;
    } else {
        liot_trace("[HEX] CRC check failed: calc=0x%X (%d), recv=0x%X (%d)", 
                   calculated_crc, calculated_crc, received_crc, received_crc);
        return 0;
    }
}

/******************************************************************************************************
 * 名    称：hex_protocol_parse
 * 功    能：解析 Hex 协议帧
 * 入口参数：buf - 数据缓冲区，len - 数据长度，frame - 输出帧结构
 * 出口参数：返回 1 表示解析成功，返回 0 表示解析失败
 * 说    明：根据命令码自动选择解析方式
 *****************************************************************************************************/
int hex_protocol_parse(const uint8_t *buf, uint8_t len, hex_protocol_frame_t *frame)
{
    if (!buf || !frame || len < HEX_PROTOCOL_MIN_FRAME_LEN) {
        return 0;
    }
    
    /* 检查起始符 */
    if (buf[0] != 0xAA) {
        liot_trace("[HEX] Invalid start byte: %d", buf[0]);
        return 0;
    }
    
    /* 填充帧结构 */
    frame->start = buf[0];
    frame->cmd = buf[1];
    
    /* 根据命令码选择解析方式 */
    switch (frame->cmd) {
        case HEX_CMD_BASIC:
            if (len < 5) {
                liot_trace("[HEX] Basic command length error: %d", len);
                return 0;
            }
            frame->mode = buf[2];
            frame->status = buf[3];
            frame->crc = buf[4];
            break;
            
        case HEX_CMD_EXTENDED:
            if (len < 10) {
                liot_trace("[HEX] Extended command length error: %d", len);
                return 0;
            }
            frame->mode = buf[2];
            frame->status = buf[3];
            /* 电压：高字节在前，低字节在后 */
            frame->voltage = ((uint16_t)buf[4] << 8) | buf[5];
            /* 电流：高字节在前，低字节在后 */
            frame->current = ((uint16_t)buf[6] << 8) | buf[7];
            frame->fault = buf[8];
            frame->crc = buf[9];
            break;
            
        case HEX_CMD_FEEDBACK:
            if (len < 5) {
                liot_trace("[HEX] Feedback command length error: %d", len);
                return 0;
            }
            frame->mode = buf[2];
            frame->status = buf[3];
            frame->crc = buf[4];
            break;
            
        default:
            liot_trace("[HEX] Unknown command: %d", frame->cmd);
            return 0;
    }
    
    liot_trace("[HEX] Parse success: CMD=%X (%s), MODE=%X (%s), STATUS=%X (%s)", 
               frame->cmd, hex_get_cmd_name(frame->cmd),
               frame->mode, hex_get_mode_name(frame->mode),
               frame->status, hex_get_status_name(frame->mode, frame->status));
    
    return 1;
}

/******************************************************************************************************
 * 名    称：is_hex_protocol
 * 功    能：判断是否为 Hex 协议
 * 入口参数：buf - 数据缓冲区，len - 数据长度
 * 出口参数：返回 1 表示是 Hex 协议，返回 0 表示不是
 * 说    明：根据起始符和帧长度判断
 *****************************************************************************************************/
int is_hex_protocol(const uint8_t *buf, uint8_t len)
{
    if (!buf || len < HEX_PROTOCOL_MIN_FRAME_LEN) {
        return 0;
    }
    
    /* Hex 协议起始符为 0xAA，且第二个字节为命令码（0x01/0x02） */
    if (buf[0] == 0xAA && (buf[1] == 0x01 || buf[1] == 0x02)) {
        /* 进一步验证帧长度 */
        if (buf[1] == 0x01 && len == 5) {
            return 1;  /* 基本命令 */
        } else if (buf[1] == 0x02 && len == 10) {
            return 1;  /* 扩展命令 */
        }
    }
    
    return 0;
}

/******************************************************************************************************
 * 名    称：hex_to_ascii_convert
 * 功    能：将 Hex 协议帧转换为 ASCII UART 协议格式
 * 入口参数：hex_frame - Hex 帧结构，ascii_data - 输出 ASCII 数据字符串
 * 出口参数：返回 1 表示转换成功，返回 0 表示转换失败
 * 说    明：
 *  - 电机模式 (HOLD/JOG): 转换为 CMD='8' 格式
 *  - 开关模式 (SWITCH): 转换为 CMD='5' 格式，status 按 bit 解析为 4 路 IO 状态
 *****************************************************************************************************/
int hex_to_ascii_convert(hex_protocol_frame_t *hex_frame, char *ascii_data)
{
    char mode_char;
    char status_char;
    char fault_char;
    char voltage_str[6];
    char current_str[6];
    
    if (!hex_frame || !ascii_data) {
        return 0;
    }
    
    /* 【分支 1】电机模式：HOLD 或 JOG，执行原有逻辑 */
    if (hex_frame->mode == HEX_MODE_HOLD || hex_frame->mode == HEX_MODE_JOG) {
        /* 工作模式转换：0x00='L', 0x01='J' */
        switch (hex_frame->mode) {
            case HEX_MODE_HOLD:
                mode_char = 'L';
                break;
            case HEX_MODE_JOG:
                mode_char = 'J';
                break;
            default:
                liot_trace("[HEX] Invalid mode: %d", hex_frame->mode);
                return 0;
        }
        
        /* 开关状态转换：0x01='T', 0x02='F', 0x03='R' */
        switch (hex_frame->status) {
            case HEX_STATUS_STOP:
                status_char = 'T';
                break;
            case HEX_STATUS_FORWARD:
                status_char = 'F';
                break;
            case HEX_STATUS_REVERSE:
                status_char = 'R';
                break;
            default:
                liot_trace("[HEX] Invalid status: %d", hex_frame->status);
                return 0;
        }
        
        /* 电压转换：假设 hex_frame->voltage 已经是整数 V */
        /* 强制输出为 "XXX.0" 格式 */
        sprintf(voltage_str, "%03d.0", hex_frame->voltage);

        /* 电流转换：不使用 float */
        uint16_t curr_integer = hex_frame->current / 10;    /* 获取整数部分 */
        uint16_t curr_decimal = hex_frame->current % 10;    /* 获取小数部分 */

        /* 格式化为 "XXX.X"：使用 %03d 保证整数部分占 3 位，前面补 0 */
        sprintf(current_str, "%03d.%1d", curr_integer, curr_decimal);

        /* 故障码转换：Hex 故障码映射到 UART 故障码 */
        switch (hex_frame->fault) {
            case HEX_FAULT_NONE:
                fault_char = '0';
                break;
            case HEX_FAULT_TIMING_UP:
                fault_char = '1';
                break;
            case HEX_FAULT_UNDER_VOLTAGE:
                fault_char = '2';
                break;
            case HEX_FAULT_OVER_VOLTAGE:
                fault_char = '3';
                break;
            case HEX_FAULT_NOLOAD:
                fault_char = '6';
                break;
            case HEX_FAULT_OVERLOAD:
                fault_char = '7';
                break;
            case HEX_FAULT_LEAKAGE:
                fault_char = '8';
                break;
            case HEX_FAULT_PHASE_A:
                fault_char = 'A';
                break;
            case HEX_FAULT_PHASE_B:
                fault_char = 'B';
                break;
            case HEX_FAULT_PHASE_C:
                fault_char = 'C';
                break;
            case HEX_FAULT_SHORT_CIRCUIT:
                fault_char = 'D';
                break;
            default:
                fault_char = '0';  /* 未知故障码映射为无故障 */
                break;
        }
        
        /* 构建 ASCII DATA 字段（电机模式：CMD='8'） */
        /* 格式：模式 + 状态 + V 电压 + A 电流 + 保护开关 (2 字符) + 故障码 */
        sprintf(ascii_data, "%c%cV%sA%sNN%c", 
                mode_char, status_char, 
                voltage_str, current_str, 
                fault_char);
        
        liot_trace("[HEX] Motor mode convert to ASCII: %s", ascii_data);
        return 1;
    }
    
    /* 【分支 2】开关模式：SWITCH，按 bit 解析 status */
    else if (hex_frame->mode == HEX_MODE_SWITCH) {
        /* 电压转换：假设 hex_frame->voltage 已经是整数 V */
        sprintf(voltage_str, "%03d.0", hex_frame->voltage);

        /* 电流转换：不使用 float */
        uint16_t curr_integer = hex_frame->current / 10;    /* 获取整数部分 */
        uint16_t curr_decimal = hex_frame->current % 10;    /* 获取小数部分 */
        sprintf(current_str, "%03d.%1d", curr_integer, curr_decimal);

        /* 故障码转换：Hex 故障码映射到 UART 故障码 */
        switch (hex_frame->fault) {
            case HEX_FAULT_NONE:
                fault_char = '0';
                break;
            case HEX_FAULT_TIMING_UP:
                fault_char = '1';
                break;
            case HEX_FAULT_UNDER_VOLTAGE:
                fault_char = '2';
                break;
            case HEX_FAULT_OVER_VOLTAGE:
                fault_char = '3';
                break;
            case HEX_FAULT_NOLOAD:
                fault_char = '6';
                break;
            case HEX_FAULT_OVERLOAD:
                fault_char = '7';
                break;
            case HEX_FAULT_LEAKAGE:
                fault_char = '8';
                break;
            case HEX_FAULT_PHASE_A:
                fault_char = 'A';
                break;
            case HEX_FAULT_PHASE_B:
                fault_char = 'B';
                break;
            case HEX_FAULT_PHASE_C:
                fault_char = 'C';
                break;
            case HEX_FAULT_SHORT_CIRCUIT:
                fault_char = 'D';
                break;
            default:
                fault_char = '0';  /* 未知故障码映射为无故障 */
                break;
        }
        
        /* 【关键】按 bit 解析 status，生成 4 位 IO 状态字符串 */
        char io_status[5];  /* 4 位 IO 状态 + '\0' */
        io_status[0] = (hex_frame->status & 0x08) ? '1' : '0';  /* bit3: IO4 (最高位) */
        io_status[1] = (hex_frame->status & 0x04) ? '1' : '0';  /* bit2: IO3 */
        io_status[2] = (hex_frame->status & 0x02) ? '1' : '0';  /* bit1: IO2 */
        io_status[3] = (hex_frame->status & 0x01) ? '1' : '0';  /* bit0: IO1 (最低位) */
        io_status[4] = '\0';
        
        liot_trace("[HEX] Switch mode IO status: %s", io_status);
        
        /* 构建 ASCII DATA 字段（开关模式：CMD='5'） */
        /* 格式：V 电压 + A 电流 + 4 位 IO 状态 + 保护开关 (2 字符) + 故障码 */
        sprintf(ascii_data, "V%sA%s%sNN%c", 
                voltage_str, current_str, 
                io_status,
                fault_char);
        
        liot_trace("[HEX] Switch mode convert to ASCII: %s", ascii_data);
        return 1;
    }
    
    /* 【分支 3】无效模式 */
    else {
        liot_trace("[HEX] Invalid mode: %d", hex_frame->mode);
        return 0;
    }
}

/******************************************************************************************************
 * 名    称：hex_parse_basic_command
 * 功    能：解析并处理 Hex 基本命令
 * 入口参数：frame - Hex 帧结构
 * 出口参数：返回 1 表示处理成功，返回 0 表示处理失败
 * 说    明：
 *  - 电机模式 (HOLD/JOG): 处理正反转控制
 *  - 开关模式 (SWITCH): 暂不支持，基本命令仅用于电机控制
 * 修    改：26-04-07 适配正反转MCU返回相同模组推送数据，导致HEX推送String字符串问题
 *****************************************************************************************************/
static int hex_parse_basic_command(hex_protocol_frame_t *frame)
{
    // 这里为了兼容 V4.4.0 版本，平台下发模组发送数据后MCU会原封不动反馈，这里有限判断是否为平台下发数据，如果是则置标志位后返回1
    if (uart2.cmd7_platform_rcv_cnt < TIME_PLATFORM_CMD_PROTECT_2S) {   // 平台下行控制，不处理
        // MCU 反馈该数据，而不是0x0F，所以在这里设置为1
        uart2.isIoSwitchFeedback = 1;  // 复用标志位
        liot_trace("-------> UART2 Motor control 0x01 received (HEX) uart2.isIoSwitchFeedback = 1 <-------");
        return 1;   // 已处理
    }

    char ascii_data[3];
    char mode_char, status_char;
    
    /* 【入口判断】检查是否为开关模式 */
    if (frame->mode == HEX_MODE_SWITCH) {
        liot_trace("[HEX] Basic command not support switch mode (mode=0x02)");
        liot_trace("[HEX] Please use extended command (0x02) for switch control");
        return 0;
    }
    
    /* 1. 确定模式字符：HOLD='L', JOG='J' */
    mode_char = (frame->mode == HEX_MODE_HOLD) ? 'L' : 'J';
    
    /* 2. 确定状态字符：STOP='T', FORWARD='F', REVERSE='R' */
    switch (frame->status) {
        case HEX_STATUS_STOP:
            status_char = 'T';
            break;
        case HEX_STATUS_FORWARD:
            status_char = 'F';
            break;
        case HEX_STATUS_REVERSE:
            status_char = 'R';
            break;
        default:
            liot_trace("[HEX] Basic command invalid status: %d", frame->status);
            return 0;
    }
    
    /* 构建 2 字符 ASCII 数据 */
    ascii_data[0] = mode_char;
    ascii_data[1] = status_char;
    ascii_data[2] = '\0';
    
    /* 4. 修正打印语句（确保参数一一对应） */
    liot_trace("[HEX] Basic command: mode=%c, status=%c | name: mode=%s, status=%s", 
                mode_char, 
                status_char, 
                hex_get_mode_name(frame->mode),
                hex_get_status_name(frame->mode, frame->status));

    /* 调用 CMD7 处理函数 (不发送 ASCII ACK) */
    liot_trace("HEX to ASCII Result: %s", ascii_data);  // 打印防止编译错误
    drv_handle_cmd_7(ascii_data, "00", 0);  /* send_ack=0 不发送 ASCII ACK */
    
    /* 兼容 V4.4.0 版本，收到 MCU 不在回复ACK */
    // hex_send_basic_ack(frame->status);

    return 1;
}

/******************************************************************************************************
 * 名    称：hex_parse_extended_command
 * 功    能：解析并处理 Hex 扩展命令
 * 入口参数：frame - Hex 帧结构
 * 出口参数：返回 1 表示处理成功，返回 0 表示处理失败
 * 说    明：
 *  - 电机模式 (HOLD/JOG): 调用 CMD='8' 处理
 *  - 开关模式 (SWITCH): 调用 CMD='5' 处理
 *****************************************************************************************************/
static int hex_parse_extended_command(hex_protocol_frame_t *frame)
{
    char ascii_data[32];
    
    /* 转换为 ASCII 格式 */
    if (!hex_to_ascii_convert(frame, ascii_data)) {
        liot_trace("[HEX] Extended command convert failed");
        return 0;
    }
    
    liot_trace("[HEX] Extended command: %s (mode=%d)", ascii_data, frame->mode);
    
    /* 【关键】根据模式调用不同的处理函数 */
    if (frame->mode == HEX_MODE_SWITCH) {
        /* 开关模式：调用 CMD='5' 处理函数（参数心跳/电压/电流/故障） */
        liot_trace("[HEX] Switch mode: calling CMD5 handler");
        drv_handle_cmd_5(ascii_data, "00", 0);  /* send_ack=0 不发送 ASCII ACK */
    } else {
        /* 电机模式：调用 CMD='8' 处理函数 */
        liot_trace("[HEX] Motor mode: calling CMD8 handler");
        drv_handle_cmd_8(ascii_data, "00", 0);  /* send_ack=0 不发送 ASCII ACK */
    }
    
    /* 发送 Hex ACK（HEX正反转没有回复，HEX开关暂时也不开启回复） */
    // hex_send_extended_ack();
    
    return 1;
}

/******************************************************************************************************
 * 名    称：hex_handle_basic_command
 * 功    能：处理 Hex 基本命令
 * 入口参数：frame - Hex 帧结构
 * 说    明：基本命令对应电机正反转控制
 *****************************************************************************************************/
static void hex_handle_basic_command(hex_protocol_frame_t *frame)
{
    liot_trace("[HEX] Handle basic command: mode=%d (%s), status=%d (%s)", 
               frame->mode, hex_get_mode_name(frame->mode),
               frame->status, hex_get_status_name(frame->mode, frame->status));
    
    /* 解析并处理 */
    hex_parse_basic_command(frame);
}

/******************************************************************************************************
 * 名    称：hex_handle_extended_command
 * 功    能：处理 Hex 扩展命令
 * 入口参数：frame - Hex 帧结构
 * 说    明：扩展命令对应电机状态 + 电压电流上报
 *****************************************************************************************************/
static void hex_handle_extended_command(hex_protocol_frame_t *frame)
{
    liot_trace("[HEX] Handle extended command: mode=%d (%s), status=%d (%s), volt=%d, curr=%d, fault=0x%X (%s)", 
               frame->mode, hex_get_mode_name(frame->mode),
               frame->status, hex_get_status_name(frame->mode, frame->status),
               frame->voltage, frame->current, 
               frame->fault, hex_get_fault_name(frame->fault));
    
    /* 解析并处理 */
    hex_parse_extended_command(frame);
}

/******************************************************************************************************
 * 名    称：hex_protocol_receive_handler
 * 功    能：Hex 协议接收处理主函数
 * 入口参数：buf - 数据缓冲区，len - 数据长度
 * 说    明：接收 Hex 协议数据，解析并调用对应的处理函数
 *****************************************************************************************************/
void hex_protocol_receive_handler(const uint8_t *buf, uint8_t len)
{
    hex_protocol_frame_t frame;
    
    if (!buf || len < HEX_PROTOCOL_MIN_FRAME_LEN) {
        liot_trace("[HEX] Invalid input buffer");
        return;
    }
    
    /* 验证 CRC */
    if (!hex_protocol_verify(buf, len)) {
        liot_trace("[HEX] CRC verification failed");
        return;
    }
    
    /* 解析帧 */
    if (!hex_protocol_parse(buf, len, &frame)) {
        liot_trace("[HEX] Frame parse failed");
        return;
    }
    
    /* 根据命令码分发处理 */
    switch (frame.cmd) {
        case HEX_CMD_BASIC:
            hex_handle_basic_command(&frame);
            break;

        case HEX_CMD_EXTENDED:
            hex_handle_extended_command(&frame);
            break;

        case HEX_CMD_FEEDBACK:
            hex_handle_feedback_command(&frame);
            break;

        default:
            liot_trace("[HEX] Unknown command: %d", frame.cmd);
            break;
    }
}

/******************************************************************************************************
 * @brief 处理 Hex 反馈命令 (0x0F)
 * @param frame 解析后的帧结构
 * 
 * 反馈帧格式：AA 0F MODE STATUS CRC
 * 开关模式：AA 0F 02 IO_STATE CRC
 * 电机模式：AA 0F MODE STATUS CRC (MODE=0x00/0x01, STATUS=0x01/0x02/0x03)
 * 
 * 说明：
 * 1. 开关模式下，STATUS 字段可能同时用于 IO_STATE 和 VALUE 两种含义
 * 2. 因此 uart2.isIoSwitchFeedback 和 uart2.isMemStateFeedback 可能同时置 1
 * 3. 上层函数调用是唯一的，实际业务逻辑中只会处理一种反馈，因此不影响功能
 * 4. 电机模式下，检查 MODE 和 STATUS 是否与发送的一致
 *****************************************************************************************************/
void hex_handle_feedback_command(hex_protocol_frame_t *frame)
{
    liot_trace("[HEX] Feedback: MODE=%X STATUS=%X", frame->mode, frame->status);
    
    // 根据 mode 判断产品类型，处理对应的反馈
    if (frame->mode == HEX_MODE_SWITCH) {
        // 开关模式：检查是否为 CMD4 的反馈
        // 反馈帧格式：AA 0F 02 IO_STATE CRC
        // 其中 STATUS 字段对应 IO_STATE
        if (drv_send_cmd4_push_ack_check_hex()) {
            uart2.isIoSwitchFeedback = 1;
            liot_trace("-------> UART2 IO switch feedback received (HEX) <-------");
        }
        // 开关模式：检查是否为 CMD9 的反馈
        // 反馈帧格式：AA 0F 02 VALUE CRC
        // 其中 STATUS 字段对应 VALUE (0x00/0x01/0x02)
        if (drv_send_cmd9_push_ack_check_hex()) {
            uart2.isMemStateFeedback = 1;
            liot_trace("-------> UART2 MEM state feedback received (HEX) <-------");
        }
    }
    // 电机模式：检查是否为 CMD7 的反馈
    else if (frame->mode == HEX_MODE_HOLD || frame->mode == HEX_MODE_JOG) {
        if (drv_send_cmd7_push_ack_check_hex()) {
            uart2.isIoSwitchFeedback = 1;  // 复用标志位
            liot_trace("-------> UART2 Motor control feedback received (HEX) <-------");
        }
    }
}
