/***************************************************************************
 * 文件名: drv_uart_protocol.c
 * 描述: UART协议解析与处理实现
 * 版本: 1.0
 * 创建日期: 2026-02-22
 * 修改记录:
 * 
 **************************************************************************/


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "liot_os.h"
#include "liot_type.h"
#include "liot_uart.h"
#include "task_uart.h"
#include "hal_project.h"
#include "drv_uart_protocol.h"


IOOutputStatus      g_io;

char global_cmd3_frame[] = "AA3SWTV3.0PWR78XXFF";   /* CMD3推送帧模板，CMD='3'，网络状态占位符为'PWR'，msgID占位符为'78'，CRC占位符为'XX' */
char global_cmd4_frame[] = "AA4000031XXFF";         /* CMD4推送帧模板，CMD='4'，IO状态占位符为'0000'，msgID占位符为'31'，CRC占位符为'XX' */
char global_cmd7_frame[] = "AA7JT0033XXFF";         /* CMD7 推送帧模板，CMD='7'，电机控制占位符为'JT'（2 字符：模式 + 状态），msgID 占位符为'33'，CRC 占位符为'XX' */
char global_cmd9_frame[] = "AA9MEM000032XXFF";      /* CMD9推送帧模板，CMD='9'，状态记忆占位符为'MEM'，msgID占位符为'32'，CRC占位符为'XX' */

/****************************************************************************************
 * 名    称：drv_send_cmd3_push_net_status
 * 功    能：每10ms调用(在cat.1初始化上层函数中）：针对 cat1.reg_status 进行 CMD='3' UART消息推送
 * 规则：
 *  - 若 reg_status 发生变化，立即发送对应状态的 CMD3 帧，计数清0
 *  - 若状态未变，则累加本地计数器，达到 2s（count>=2）时发送一次，并清0
 * 修    改：
 ***************************************************************************************/
static uart_net_status_e net_last_status = uart_net_status_FAL;             // 上次发送的网络状态，初始值为FAL
void drv_send_cmd3_push_net_status(void)
{
    /* map cat1.reg_status -> uart_net_status_e */
    uart_net_status_e net_curr_status = uart_net_status_PWR;
    switch (cat1.reg_status) {
        case enCAT1_MODULE_FW: net_curr_status = uart_net_status_PWR; break;
        case enCAT1_IMEI:     net_curr_status = uart_net_status_PWR; break;
        case enCAT1_ICCID:    net_curr_status = uart_net_status_SIM; break;
        case enCAT1_NET_ACCESS: net_curr_status = uart_net_status_NET; break;
        case enCAT1_CLOUD_REG:  net_curr_status = uart_net_status_CLD; break;
        case enCAT1_INIT_SUC:   net_curr_status = uart_net_status_SUC; break;
        case enCAT1_INIT_FAIL:  net_curr_status = uart_net_status_FAL; break;
        default: net_curr_status = uart_net_status_PWR; break;
    }

    /* table for enum -> 3-char code */
    static const char *const net_code[] = { "PWR", "SIM", "NET", "CLD", "SUC", "FAL" };

    /* send immediately on status change */
    if (net_curr_status != net_last_status) {
        drv_send_cmd3_push((char *)net_code[net_curr_status]);
        uart2.cmd3_sec_cnt = 0;
        net_last_status = net_curr_status;
        return;
    }

    /* otherwise send every 2s */
    if (uart2.cmd3_sec_cnt >= 2) {
        drv_send_cmd3_push((char *)net_code[net_curr_status]);
        uart2.cmd3_sec_cnt = 0;
    }
}


/****************************************************************************************
 * 名   称：drv_send_cmd3_push
 * 功   能：发送CMD='3'状态帧，data为3字符网络状态（例如"PWR"/"SIM"/"NET"/"CLD"/"SUC"/"FAL"）
 * 入口参数：network_status - 3字符网络状态字符串
 * 出口参数：无
 * 示    例：drv_send_cmd3_push("PWR")
 * 上电网络状态推送示例：AA3SWTV3.0PWR0225FF
 *                     AA3SWTV3.0SIM0301FF
 *                     AA3SWTV3.0NET0494FF
 *                     AA3SWTV3.0CLD0587FF
 *                     AA3SWTV3.0SUC0614FF
 ***************************************************************************************/
void drv_send_cmd3_push(char *network_status)
{
    /* 更新网络状态：替换"PWR"的位置（10-12）为传入的network_status */
    if (network_status && strlen(network_status) >= 3) {
        global_cmd3_frame[10] = network_status[0];
        global_cmd3_frame[11] = network_status[1];
        global_cmd3_frame[12] = network_status[2];
    }

    /* 自增messageIDCounter */
    uart2.messageIDCounter++;
    if (uart2.messageIDCounter > 99) uart2.messageIDCounter = 1;        // 循环0-99，避免溢出
    sprintf((char *)uart2.messageID, "%02d", uart2.messageIDCounter);   // 更新uart2.messageID为两位ASCII
    global_cmd3_frame[13] = uart2.messageID[0];                         // 更新global_cmd3_frame的msgID
    global_cmd3_frame[14] = uart2.messageID[1];

    /* 计算CRC：从AA到msgID（位置0-16） */
    unsigned char crc_result[3];
    uart_protocol_calculate_crc((unsigned char*)global_cmd3_frame, 17, crc_result);
    global_cmd3_frame[15] = crc_result[0];
    global_cmd3_frame[16] = crc_result[1];

    /* 发送推送帧 (只发有效字符，不包含字符串终止符) */
    liot_uart_write(LIOT_UART_PORT_2, (unsigned char*)global_cmd3_frame, (int)strlen(global_cmd3_frame));
    liot_trace("[DRV] Sent CMD3 push frame: %s", global_cmd3_frame);
}


/****************************************************************************************
 * 名   称：drv_send_cmd4_push
 * 功   能：发送CMD='4'状态帧，data为4字符IO状态（例如"0000"/"1111"）
 * 入口参数：无
 * 出口参数：无
 * 示    例：drv_send_cmd4_push_str()
 * UART字符串推送示例："AA4000031XXFF"
 ***************************************************************************************/
void drv_send_cmd4_push_str()
{
    /* 更新IO状态：替换'0000'的位置（3-6）为传入的io_status */
    global_cmd4_frame[3] = (g_io.out[0].now_status == enIO_OUT_ON) ? '1' : '0';
    global_cmd4_frame[4] = (g_io.out[1].now_status == enIO_OUT_ON) ? '1' : '0';
    global_cmd4_frame[5] = (g_io.out[2].now_status == enIO_OUT_ON) ? '1' : '0';
    global_cmd4_frame[6] = (g_io.out[3].now_status == enIO_OUT_ON) ? '1' : '0';

    /* 自增messageIDCounter */
    uart2.messageIDCounter++;
    if (uart2.messageIDCounter > 99) uart2.messageIDCounter = 1;        // 循环0-99，避免溢出
    sprintf((char *)uart2.messageID, "%02d", uart2.messageIDCounter);   // 更新uart2.messageID为两位ASCII
    global_cmd4_frame[7] = uart2.messageID[0];                          // 更新global_cmd4_frame的msgID
    global_cmd4_frame[8] = uart2.messageID[1];

    /* 计算CRC：从AA到msgID（位置0-8） */
    unsigned char crc_result[3];
    uart_protocol_calculate_crc((unsigned char*)global_cmd4_frame, 9, crc_result);
    global_cmd4_frame[9] = crc_result[0];
    global_cmd4_frame[10] = crc_result[1];

    /* 发送推送帧 (只发有效字符，不包含字符串终止符) */
    liot_uart_write(LIOT_UART_PORT_2, (unsigned char*)global_cmd4_frame, (int)strlen(global_cmd4_frame));
    liot_trace("[DRV] Sent CMD4 push frame: %s", global_cmd4_frame);
}

/****************************************************************************************
 * 名   称：drv_send_cmd4_push_ack_check_str
 * 功   能：检查CMD='4'推送后的反馈是否合法，通过检查uart2.rcvBuff中的数据实现
 * 入口参数：无
 * 出口参数：返回1表示收到合法反馈，返回0表示未收到或反馈不合法
 * 示   例：
 *         - CMD4推送帧："AA4000031XXFF"
 *         - 合法反馈帧示例："AAF4031XXFF"
 * 修   改：
 ***************************************************************************************/
int drv_send_cmd4_push_ack_check_str() {
    // 检查反馈位是否为 'F'
    if (uart2.rcvBuff[2] != 'F') {
        return 0; // 无效反馈
    }
    liot_trace("UART2 IO switch feedback received, code=%c", uart2.rcvBuff[2]);

    if (uart2.rcvBuff[3] != '4') {
        return 0; // 非CMD4反馈
    }
    liot_trace("UART2 IO switch feedback for CMD4 received, code=%c", uart2.rcvBuff[3]);

    if(uart2.rcvBuff[4] != '0') {
        return 0; // 非成功结果反馈
    }
    liot_trace("UART2 IO switch feedback indicates success, code=%c", uart2.rcvBuff[4]);

    // 检查校验位
    if (!uart_protocol_check_protocol(uart2.rcvBuff, uart2.receivedLength)) {
        liot_trace("UART2 IO switch feedback CRC check failed");
        return 0; // 校验失败
    }

    liot_trace("UART2 IO switch feedback CRC check passed");
    return 1; // 反馈合法
}

/******************************************************************************************************
 * 名    称：drv_send_cmd4_push_hex
 * 功    能：发送 Hex 协议 CMD='4' 开关状态推送帧
 * 入口参数：无
 * 出口参数：无
 * 说    明：Hex 格式：AA 04 02 IO_STATE CRC
 *           - 0x04: CMD=4 (开关状态推送)
 *           - 0x02: 固定值 (开关模式标识)
 *           - IO_STATE: bit0-bit3 对应 IO1-IO4 状态
 * 示    例：AA 04 02 01 AC (IO1=ON, 其他 OFF)
 *****************************************************************************************************/
void drv_send_cmd4_push_hex(void)
{
    uint8_t hex_frame[5];
    uint8_t io_state = 0;
    
    /* 构建 IO 状态字节：g_io.out[3] 对应 bit0，g_io.out[0] 对应 bit3 */
    io_state |= (g_io.out[3].now_status == enIO_OUT_ON) ? 0x01 : 0x00;  // bit0: IO1
    io_state |= (g_io.out[2].now_status == enIO_OUT_ON) ? 0x02 : 0x00;  // bit1: IO2
    io_state |= (g_io.out[1].now_status == enIO_OUT_ON) ? 0x04 : 0x00;  // bit2: IO3
    io_state |= (g_io.out[0].now_status == enIO_OUT_ON) ? 0x08 : 0x00;  // bit3: IO4
    
    /* 保存发送的 IO 状态，用于后续反馈检查 */
    uart2.lastSentIoState = io_state;
    
    /* 构建 Hex 帧 */
    hex_frame[0] = 0xAA;                    // 起始符
    hex_frame[1] = 0x01;                    // CMD=4 对应 HEX 命令为 0x01
    hex_frame[2] = 0x02;                    // 固定值 (开关模式标识)
    hex_frame[3] = io_state;                // IO 状态
    hex_frame[4] = (hex_frame[0] + hex_frame[1] + hex_frame[2] + hex_frame[3]) & 0xFF;  // CRC
    
    /* 发送 */
    liot_uart_write(LIOT_UART_PORT_2, hex_frame, 5);
    liot_trace("[HEX] Sent CMD4 push: AA 04 02 %X %X", io_state, hex_frame[4]);
}

/******************************************************************************************************
 * 名    称：drv_send_cmd4_push_ack_check_hex
 * 功    能：检查 CMD4 推送后的 HEX 反馈是否合法（只检查第 4 字节 IO 状态）
 * 入口参数：无（通过 uart2.rcvBuff 读取反馈数据）
 * 返    回：1-反馈合法，0-反馈非法
 * 说    明：
 *  - 反馈帧格式：AA 0F 02 IO_STATE CRC
 *  - 注意：调用此函数前已经检查过：
 *      1. CRC 校验（hex_protocol_verify）
 *      2. 起始符 0xAA（hex_protocol_parse）
 *      3. 命令码 0x0F（hex_protocol_parse）
 *      4. 帧长度 >= 5（hex_protocol_parse）
 *      5. MODE == 0x02（hex_handle_feedback_command）
 *  - 本函数只检查第 4 字节 IO_STATE 是否与发送的一致
 *  - IO_STATE: bit0-bit3 对应 IO1-IO4 状态
 * 示    例：AA 0F 02 01 BA (IO1=ON, 反馈成功)
 *****************************************************************************************************/
int drv_send_cmd4_push_ack_check_hex(void)
{
    /* 检查第 4 字节 IO 状态是否与发送的一致 */
    /* 注意：uart2.lastSentIoState 在发送时已保存 */
    if (uart2.rcvBuff[3] != uart2.lastSentIoState) {
        liot_trace("[HEX] CMD4 feedback mismatch: sent=0x%X, recv=0x%X", 
                   uart2.lastSentIoState, uart2.rcvBuff[3]);
        return 0;
    }
    
    liot_trace("[HEX] CMD4 push ack received: IO_STATE=0x%X", uart2.rcvBuff[3]);
    
    return 1;
}

/*****************************************************************************************************
 * 名   称：drv_send_cmd9_push_ack_check_hex
 * 功   能：检查 CMD9 推送后的 HEX 反馈是否合法（只检查第 4 字节 VALUE）
 * 入口参数：无（通过 uart2.rcvBuff 读取反馈数据）
 * 返    回：1-反馈合法，0-反馈非法
 * 说    明：
 *  - 反馈帧格式：AA 0F 02 VALUE CRC
 *  - 前面已经检查过起始符、CMD、MODE，这里只检查第 4 字节 VALUE 是否与发送的一致
 *  - VALUE: 0x00(上电关)/0x01(上电开)/0x02(上电保持)
 * 示    例：
 *  - AA 0F 02 00 BB (上电关，反馈成功)
 *  - AA 0F 02 01 BC (上电开，反馈成功)
 *  - AA 0F 02 02 BD (上电保持，反馈成功)
 *****************************************************************************************************/
int drv_send_cmd9_push_ack_check_hex(void)
{
    /* 检查第 4 字节 VALUE 是否与发送的一致 */
    /* 注意：uart2.lastSentMemValue 在发送时已保存 */
    if (uart2.rcvBuff[3] != uart2.lastSentMemValue) {
        liot_trace("[HEX] CMD9 feedback mismatch: sent=0x%X, recv=0x%X", 
                   uart2.lastSentMemValue, uart2.rcvBuff[3]);
        return 0;
    }
    
    liot_trace("[HEX] CMD9 push ack received: VALUE=0x%X (MEM state)", uart2.rcvBuff[3]);
    
    return 1;
}

/****************************************************************************************
 * 名   称：drv_send_cmd7_push_str
 * 功   能：发送 CMD='7' 状态帧（ASCII 协议版本），data 为 2 字符电机状态（模式 + 状态，例如"JT"/"JF"/"JR"/"LT"/"LF"/"LR"）
 * 入口参数：无
 * 出口参数：无
 * 示    例：drv_send_cmd7_push_str()
 * UART 字符串推送示例："AA7JT0033XXFF"
 ***************************************************************************************/
void drv_send_cmd7_push_str()
{
    /* 更新电机控制状态：替换'JT'的位置（3-4）为实际的电机控制码 */
    char motor_control[2];
    
    #if PRODUCT_MOTOR
    /* 根据 motor.currentState 生成对应的 2 字符控制码 */
    switch (motor.currentState) {
        case MODE_FORWARD_JOG:
            motor_control[0] = 'J';
            motor_control[1] = 'F';
            break;
        case MODE_REVERSE_JOG:
            motor_control[0] = 'J';
            motor_control[1] = 'R';
            break;
        case MODE_IDLE_JOG:
            motor_control[0] = 'J';
            motor_control[1] = 'T';
            break;
        case MODE_FORWARD_HOLD:
            motor_control[0] = 'L';
            motor_control[1] = 'F';
            break;
        case MODE_REVERSE_HOLD:
            motor_control[0] = 'L';
            motor_control[1] = 'R';
            break;
        case MODE_IDLE_HOLD:
            motor_control[0] = 'L';
            motor_control[1] = 'T';
            break;
        default:
            motor_control[0] = 'J';
            motor_control[1] = 'T';
            break;
    }
    #else
    /* 默认值 */
    motor_control[0] = 'J';
    motor_control[1] = 'T';
    #endif
    
    global_cmd7_frame[3] = motor_control[0];
    global_cmd7_frame[4] = motor_control[1];

    /* 自增 messageIDCounter */
    uart2.messageIDCounter++;
    if (uart2.messageIDCounter > 99) uart2.messageIDCounter = 1;        // 循环 0-99，避免溢出
    sprintf((char *)uart2.messageID, "%02d", uart2.messageIDCounter);   // 更新 uart2.messageID 为两位 ASCII
    global_cmd7_frame[5] = uart2.messageID[0];                          // 更新 global_cmd7_frame 的 msgID
    global_cmd7_frame[6] = uart2.messageID[1];

    /* 计算 CRC：从 AA 到 msgID（位置 0-6） */
    unsigned char crc_result[3];
    uart_protocol_calculate_crc((unsigned char*)global_cmd7_frame, 7, crc_result);
    global_cmd7_frame[7] = crc_result[0];
    global_cmd7_frame[8] = crc_result[1];

    /* 发送推送帧 (只发有效字符，不包含字符串终止符) */
    liot_uart_write(LIOT_UART_PORT_2, (unsigned char*)global_cmd7_frame, (int)strlen(global_cmd7_frame));
    liot_trace("[DRV] Sent CMD7 push frame (ASCII): %s", global_cmd7_frame);
}

/****************************************************************************************
 * 名   称：drv_send_cmd7_push_hex
 * 功   能：发送 CMD=0x01 基本命令（Hex 协议版本，电机控制）
 * 入口参数：无
 * 出口参数：无
 * 说    明：
 *  - 根据《UART 通信协议_HEX_V2.2.pdf》第 3.1 节定义
 *  - 帧格式：AA + 01 + MODE + STATUS + CRC（5 字节）
 *  - MODE：工作模式（0x00=HOLD 联动，0x01=JOG 点动）
 *  - STATUS：电机状态（0x01=停止，0x02=正转，0x03=反转）
 *  - CRC = 前 4 字节累加取低 8 位
 * 状态映射：
 *  - MODE_FORWARD_JOG  → MODE=0x01, STATUS=0x02
 *  - MODE_REVERSE_JOG  → MODE=0x01, STATUS=0x03
 *  - MODE_IDLE_JOG     → MODE=0x01, STATUS=0x01
 *  - MODE_FORWARD_HOLD → MODE=0x00, STATUS=0x02
 *  - MODE_REVERSE_HOLD → MODE=0x00, STATUS=0x03
 *  - MODE_IDLE_HOLD    → MODE=0x00, STATUS=0x01
 ****************************************************************************************/
void drv_send_cmd7_push_hex(void)
{
    uint8_t frame[5];
    uint8_t mode, status;
    
    /* 1. 根据 motor.currentState 解析 MODE 和 STATUS */
    #if PRODUCT_MOTOR
    switch (motor.currentState) {
        case MODE_FORWARD_JOG:
            mode = 0x01;  // JOG 点动模式
            status = 0x02;  // 正转
            break;
        case MODE_REVERSE_JOG:
            mode = 0x01;  // JOG 点动模式
            status = 0x03;  // 反转
            break;
        case MODE_IDLE_JOG:
            mode = 0x01;  // JOG 点动模式
            status = 0x01;  // 停止
            break;
        case MODE_FORWARD_HOLD:
            mode = 0x00;  // HOLD 联动模式
            status = 0x02;  // 正转
            break;
        case MODE_REVERSE_HOLD:
            mode = 0x00;  // HOLD 联动模式
            status = 0x03;  // 反转
            break;
        case MODE_IDLE_HOLD:
            mode = 0x00;  // HOLD 联动模式
            status = 0x01;  // 停止
            break;
        default:
            mode = 0x00;
            status = 0x01;
            break;
    }
    #else
    /* 默认值：联动停止 */
    mode = 0x00;
    status = 0x01;
    #endif
    
    /* 2. 保存发送的 MODE 和 STATUS，用于后续反馈检查 */
    uart2.lastSentMotorMode = mode;
    uart2.lastSentMotorStatus = status;
    
    /* 3. 构建帧 */
    frame[0] = 0xAA;          // 起始符
    frame[1] = 0x01;          // CMD: 基本命令
    frame[2] = mode;          // MODE: 工作模式
    frame[3] = status;        // STATUS: 电机状态
    
    /* 4. 计算 CRC（前 4 字节累加取低 8 位） */
    frame[4] = (frame[0] + frame[1] + frame[2] + frame[3]) & 0xFF;
    
    /* 5. 发送 */
    liot_uart_write(LIOT_UART_PORT_2, frame, 5);
    
    /* 6. 日志打印 */
    liot_trace("[DRV] Sent CMD7 push frame (HEX): AA 01 %X %X %X", mode, status, frame[4]);
}

/****************************************************************************************
 * 名   称：drv_send_cmd7_push_ack_check
 * 功   能：检查 CMD='7' 推送后的反馈是否合法，通过检查 uart2.rcvBuff 中的数据实现
 * 入口参数：无
 * 出口参数：返回 1 表示收到合法反馈，返回 0 表示未收到或反馈不合法
 * 示   例：
 *         - CMD7 推送帧："AA7JT0033XXFF"
 *         - 合法反馈帧示例："AAF7033XXFF"
 * 修   改：
 ***************************************************************************************/
int drv_send_cmd7_push_ack_check() {
    // 检查反馈位是否为 'F'
    if (uart2.rcvBuff[2] != 'F') {
        return 0; // 无效反馈
    }
    liot_trace("UART2 Motor control feedback received, code=%c", uart2.rcvBuff[2]);

    if (uart2.rcvBuff[3] != '7') {
        return 0; // 非 CMD7 反馈
    }
    liot_trace("UART2 Motor control feedback for CMD7 received, code=%c", uart2.rcvBuff[3]);

    if(uart2.rcvBuff[4] != '0') {
        return 0; // 非成功结果反馈
    }
    liot_trace("UART2 Motor control feedback indicates success, code=%c", uart2.rcvBuff[4]);

    // 检查校验位
    if (!uart_protocol_check_protocol(uart2.rcvBuff, uart2.receivedLength)) {
        liot_trace("UART2 Motor control feedback CRC check failed");
        return 0; // 校验失败
    }

    liot_trace("UART2 Motor control feedback CRC check passed");
    return 1; // 反馈合法
}

/*****************************************************************************************************
 * 名   称：drv_send_cmd7_push_ack_check_hex
 * 功   能：检查 CMD7 推送后的 HEX 反馈是否合法（只检查第 3、4 字节 MODE 和 STATUS）
 * 入口参数：无（通过 uart2.rcvBuff 读取反馈数据）
 * 返    回：1-反馈合法，0-反馈非法
 * 说    明：
 *  - 反馈帧格式：AA 0F MODE STATUS CRC
 *  - 注意：调用此函数前已经检查过：
 *      1. CRC 校验（hex_protocol_verify）
 *      2. 起始符 0xAA（hex_protocol_parse）
 *      3. 命令码 0x0F（hex_protocol_parse）
 *      4. 帧长度 >= 5（hex_protocol_parse）
 *  - 本函数只检查第 3 字节 MODE 和第 4 字节 STATUS 是否与发送的一致
 * 示    例：AA 0F 00 02 BC (联动正转，反馈成功)
 *****************************************************************************************************/
int drv_send_cmd7_push_ack_check_hex(void)
{
    /* 检查第 3 字节 MODE 是否与发送的一致 */
    if (uart2.rcvBuff[2] != uart2.lastSentMotorMode) {
        liot_trace("[HEX] CMD7 feedback mismatch: MODE sent=0x%X, recv=0x%X", 
                   uart2.lastSentMotorMode, uart2.rcvBuff[2]);
        return 0;
    }
    
    /* 检查第 4 字节 STATUS 是否与发送的一致 */
    if (uart2.rcvBuff[3] != uart2.lastSentMotorStatus) {
        liot_trace("[HEX] CMD7 feedback mismatch: STATUS sent=0x%X, recv=0x%X", 
                   uart2.lastSentMotorStatus, uart2.rcvBuff[3]);
        return 0;
    }
    
    liot_trace("[HEX] CMD7 push ack received: MODE=0x%X, STATUS=0x%X", 
               uart2.rcvBuff[2], uart2.rcvBuff[3]);
    
    return 1;
}


/****************************************************************************************************************
 * 名   称：drv_send_cmd9_push_str
 * 功   能：发送 CMD='9'状态帧（ASCII 协议版本）
 * 入口参数：func_str - 3 字节功能字符串（例如 "MEM"）
 * 说    明：
 *  - 当 func_str="MEM" 时：
 *      DATA 根据 g_io.out[] 自动生成 4 字节 "0000"/"1111"
 *  - 其他 func 填充 "NULL"
 *  - 自动生成 msgID
 *  - 自动计算 CRC
 * 示    例：drv_send_cmd9_push_str("MEM");
 ****************************************************************************************************************/
void drv_send_cmd9_push_str(const char *func_str)
{
    char frame[17] = "AA9MEM00000000FF";  // 16 字节 + '\0'

    /* 1. 固定帧头 */
    frame[0] = 'A';
    frame[1] = 'A';
    frame[2] = '9';

    /* 2. 设置 FUNC (3-5) */
    if (func_str && strlen(func_str) >= 3) {
        frame[3] = func_str[0];
        frame[4] = func_str[1];
        frame[5] = func_str[2];
    } else {
        memcpy(&frame[3], "RSV", 3);  // 默认
    }

    /* 3. 填充 DATA (6-9) */
    if (strncmp(&frame[3], "MEM", 3) == 0) {
        frame[6] = cat1.memState[0];
        frame[7] = cat1.memState[1];
        frame[8] = cat1.memState[2];
        frame[9] = cat1.memState[3];
    } else {
        memcpy(&frame[6], "NULL", 4);
    }

    /* 4. 更新 messageID (10-11) */
    uart2.messageIDCounter++;
    if (uart2.messageIDCounter > 99)
        uart2.messageIDCounter = 0;

    frame[10] = (uart2.messageIDCounter / 10) + '0';
    frame[11] = (uart2.messageIDCounter % 10) + '0';

    /* 5. 计算 CRC (0-11) */
    unsigned char crc_result[3];
    uart_protocol_calculate_crc((unsigned char*)frame, 12, crc_result);
    frame[12] = crc_result[0];
    frame[13] = crc_result[1];

    /* 6. 结尾 FF */
    frame[14] = 'F';
    frame[15] = 'F';
    frame[16] = '\0';

    /* 7. 发送 */
    liot_uart_write(LIOT_UART_PORT_2, (unsigned char*)frame, 16);

    liot_trace("[DRV] Sent CMD9 push frame (ASCII): %s", frame);
}

/****************************************************************************************************************
 * 名   称：drv_send_cmd9_push_hex
 * 功   能：发送 CMD=0x09 管理设置命令（Hex 协议版本）
 * 入口参数：无（使用 uart2.lastSentMemValue 作为设置值）
 * 说    明：
 *  - 根据《UART 通信协议_HEX_V2.2.pdf》第 3.3 节定义
 *  - 帧格式：AA + 09 + MODE + VALUE + CRC（5 字节）
 *  - MODE 固定为 0x02（开关模式）
 *  - VALUE 为 uart2.lastSentMemValue（0x00:上电关，0x01:上电开，0x02:上电保持）
 *  - CRC = 前 4 字节累加取低 8 位
 * 示    例：
 *  - uart2.lastSentMemValue = 0x00; drv_send_cmd9_push_hex();  // 上电关设置
 *  - uart2.lastSentMemValue = 0x01; drv_send_cmd9_push_hex();  // 上电开设置
 *  - uart2.lastSentMemValue = 0x02; drv_send_cmd9_push_hex();  // 上电保持设置
 ****************************************************************************************************************/
void drv_send_cmd9_push_hex(void)
{
    uint8_t frame[5];
    uint8_t crc;
    uint8_t value = uart2.lastSentMemValue;  // 使用已保存的值
    
    /* 1. 构建帧 */
    frame[0] = 0xAA;          // 起始符
    frame[1] = 0x09;          // 命令码
    frame[2] = 0x02;          // MODE: 开关模式（固定）
    frame[3] = value;         // VALUE: 设置值（0x00/0x01/0x02）
    
    /* 2. 计算 CRC（前 4 字节累加取低 8 位） */
    crc = (frame[0] + frame[1] + frame[2] + frame[3]) & 0xFF;
    frame[4] = crc;
    
    /* 3. 发送 */
    liot_uart_write(LIOT_UART_PORT_2, frame, 5);
    
    /* 4. 日志打印 */
    liot_trace("[DRV] Sent CMD9 push frame (HEX): AA 09 02 %X %X", value, crc);
}

/****************************************************************************************************************
 * 名   称：drv_send_cmd9_push_ack_check
 * 功   能：检查CMD='9'推送后的反馈是否合法，通过检查uart2.rcvBuff中的数据实现
 * 入口参数：无
 * 出口参数：返回1表示收到合法反馈，返回0表示未收到或反馈不合法
 * 示   例：
 *         - CMD9推送帧："AA9MEM000032XXFF"
 *        - 合法反馈帧示例："AAF9032XXFF"
 * 修   改：
 ****************************************************************************************************************/
int drv_send_cmd9_push_ack_check() {
    // 检查反馈位是否为 'F'
    if (uart2.rcvBuff[2] != 'F') {
        return 0; // 无效反馈
    }
    liot_trace("UART2 IO switch feedback received, code=%c", uart2.rcvBuff[2]);

    if (uart2.rcvBuff[3] != '9') {
        return 0; // 非CMD9反馈
    }
    liot_trace("UART2 IO switch feedback for CMD9 received, code=%c", uart2.rcvBuff[3]);

    if(uart2.rcvBuff[4] != '0') {
        return 0; // 非成功结果反馈
    }
    liot_trace("UART2 IO switch feedback indicates success, code=%c", uart2.rcvBuff[4]);

    // 检查校验位
    if (!uart_protocol_check_protocol(uart2.rcvBuff, uart2.receivedLength)) {
        liot_trace("UART2 IO switch feedback CRC check failed");
        return 0; // 校验失败
    }

    liot_trace("UART2 IO switch feedback CRC check passed");
    return 1; // 反馈合法
}



////////////////////////////////////////////////////////////////////////////////////////
//
//                   上方为模主动推送，下方为接收处理相关函数
//
////////////////////////////////////////////////////////////////////////////////////////



int uart_protocol_check_preamble(const unsigned char *buf, unsigned int len)
{
    if (!buf) return 0;
    if (len < UART_PROTOCOL_MIN_FRAME_LEN) {
        return 0;
    }
    return (buf[0] == 'A' && buf[1] == 'A');
}


int uart_protocol_check_protocol(const unsigned char *buf, unsigned int len)
{
    if (!buf){
        liot_trace("uart_protocol_check_protocol: invalid input (null buffer)");
        return 0;
    } 
    if (len > UART_PROTOCOL_MAX_FRAME_LEN){
        liot_trace("uart_protocol_check_protocol: invalid input (length %d exceeds max %d)", len, UART_PROTOCOL_MAX_FRAME_LEN);
        return 0;
    }
    if (buf[len - 2] != 'F' || buf[len - 1] != 'F'){
        liot_trace("uart_protocol_check_protocol: invalid frame ending (expected 'FF', got '%c%c')", buf[len - 2], buf[len - 1]);
        return 0;
    }

    int msgid_pos = len - 6;
    int crc_pos = len - 4;

    /* CRC豁免 */
    if (buf[crc_pos] == 'X' && buf[crc_pos + 1] == 'X') return 1;

    unsigned char calc_crc[3] = {0};
    uart_protocol_calculate_crc(buf, msgid_pos + 2, calc_crc);
    if (buf[crc_pos] != calc_crc[0] || buf[crc_pos + 1] != calc_crc[1]){
        // liot_trace("uart_protocol_check_protocol: CRC check failed (expected '%s', got '%c%c')", calc_crc, buf[crc_pos], buf[crc_pos + 1]); // 直接打印字符可能有问题，改为打印十六进制数值
        // 临时绕过底层bug：手动转字符串打印
        char debug_buf[32];
        int b1 = buf[crc_pos];
        int b2 = buf[crc_pos+1];

        // 这种写法最稳，只传一个参数给 liot_trace，增加打印原始字符，方便一眼看出是 '51' 还是数值 5和1
        snprintf(debug_buf, sizeof(debug_buf), "Hex:%02X %02X (Char:%c%c)", b1, b2, b1, b2);
        liot_trace("CRC Fail! Expected: %s, Got %s", calc_crc, debug_buf);

        return 0;
    }

    return 1;
}

/*****************************************************************************************************************
 * 名    称：uart_protocol_parse_fields
 * 功    能：解析UART协议帧中的CMD、DATA和msgID字段
 * 入口参数：buf-UART完整数据，len-数据长度, cmd_out-解析出的CMD字符，data_out-解析出的DATA字符串（建议长度至少64），msgid_out-解析出的msgID字符串（长度至少3）
 * 出口参数：通过指针参数返回解析结果，cmd_out返回CMD字符，data_out返回DATA字符串（以'\0'结尾），msgid_out返回msgID字符串（以'\0'结尾）
 * 示    例：对于输入帧 "AA3SWTV3.0PWR0225FF"，解析结果为 cmd_out='3'，data_out="SWTV3.0PWR0225"，msgid_out="25"
 *          对于输入帧 "AA4000031XXFF"，解析结果为 cmd_out='4'，data_out="0000"，msgid_out="31"
 *          对于输入帧 "AA5V220.0A030.00001NN016XXFF", 解析结果为 cmd_out='5'，data_out="V220.0A030.00001NN", msgid_out="16"
 * 说    明：函数内部根据协议结构计算CMD、DATA和msgID的位置和长度，进行提取和返回。DATA字段会被截断以适应data_out的大小限制，并确保以'\0'结尾。msgID字段固定为2字符，解析后也以'\0'结尾。
 * 修    改：
 ****************************************************************************************************************/
void uart_protocol_parse_fields(const unsigned char *buf, int len, char *cmd_out, char *data_out, char *msgid_out)
{
    if (!buf) return;
    int msgid_pos = len - 6;
    int data_start = 2 + 1; /* AA + CMD */
    int data_len = msgid_pos - data_start;
    if (data_len < 0) data_len = 0;

    if (cmd_out) *cmd_out = buf[2];

    if (data_out) {
        int max_len = UART_PROTOCOL_DATA_OUT_SIZE_64 - 1;
        int copy_len = (data_len < max_len) ? data_len : max_len;
        memset(data_out, 0, UART_PROTOCOL_DATA_OUT_SIZE_64);
        if (copy_len > 0) memcpy(data_out, &buf[data_start], copy_len);
        data_out[copy_len] = '\0';
    }

    if (msgid_out) {
        msgid_out[0] = buf[msgid_pos];
        msgid_out[1] = buf[msgid_pos + 1];
        msgid_out[2] = '\0';
    }
}

/*****************************************************************************************************************
 * 名    称：uart_protocol_build_ack
 * 功    能：构建ACK响应帧，包含固定帧头、CMD、结果码、msgID和CRC校验
 * 入口参数：src_cmd-源CMD字符 (例如'3'/'4'/'9'等)，ACK帧中的CMD通常与源CMD相同，表示对哪个命令的响应)
 *          result-结果码字符 0 成功 1 失败 2 参数非法 3 状态非法 4 硬件执行失败 5 Busy 6 执行超时
 *          msgid-原帧中的消息ID字符串（长度至少2）
 *          out_buf-输出缓冲区
 *          out_len-输出帧长度
 * 出口参数：通过out_buf返回构建好的ACK帧，out_len返回ACK帧的实际长度
 * 示    例：对于输入 src_cmd='4', result='S', msgid="31"，构建的ACK帧为 "AAF4031XXFF"，其中CMD='4'，结果码='S'，msgID='31'，CRC根据前面部分计算得出
 * 说    明：函数内部按照协议格式构建ACK帧，固定帧头为 "AAF"，CMD和结果码根据输入参数设置，msgID使用输入的msgid参数，如果msgid无效则填充 "00"。最后计算CRC并附加到帧中，确保输出的ACK帧符合协议要求。
 * 修    改：
 *****************************************************************************************************************/

void uart_protocol_build_ack(char src_cmd, char result, const char *msgid, unsigned char *out_buf, int *out_len)
{
    if (!out_buf || !out_len) return;
    int pos = 0;
    out_buf[pos++] = 'A';
    out_buf[pos++] = 'A';
    out_buf[pos++] = 'F';
    out_buf[pos++] = (unsigned char)src_cmd;
    out_buf[pos++] = (unsigned char)result;
    if (msgid && strlen(msgid) >= 2) {
        out_buf[pos++] = (unsigned char)msgid[0];
        out_buf[pos++] = (unsigned char)msgid[1];
    } else {
        out_buf[pos++] = '0'; out_buf[pos++] = '0';
    }
    /* CRC */
    unsigned char crc_result[3];
    uart_protocol_calculate_crc(out_buf, pos, crc_result);
    out_buf[pos++] = crc_result[0];
    out_buf[pos++] = crc_result[1];

    out_buf[pos++] = 'F';
    out_buf[pos++] = 'F';
    *out_len = pos;
}


/***************************************************************************
 * 名    称：uart_calculate_crc_sum
 * 功    能：CRC求和(字符串求和)，对输入字节按ASCII累加并取%100，结果返回两位ASCII字符
 * 参    数：str - 输入字节数组
 *          length - 输入长度（字节数）
 *          result - 长度至少为3的缓冲区，返回两位ASCII数字及末尾"\0"
 **************************************************************************/
void uart_protocol_calculate_crc(const unsigned char *str, int length, unsigned char* result) {
    uint32_t sum = 0;                            /* 使用32位以避免累加溢出风险 */
    if (!str || length <= 0 || !result) {
        if (result) { result[0] = '0'; result[1] = '0'; result[2] = '\0'; }
        return;
    }
    for (int i = 0; i < length; ++i) {
        sum += (uint8_t)str[i];
    }
    uint16_t v = (uint16_t)(sum % 100);
    result[0] = (v / 10) + '0';
    result[1] = (v % 10) + '0';
    result[2] = '\0';
    liot_trace("calculate CRC is: %s", result);
}


/****************************************************************************************
 * 名    称：drv_handle_cmd_3
 * 功    能：处理UART发送CMD='3'的状态帧，反馈当前的网络状态
 * 入口参数：data - 暂未使用，msgid - 对方发送的消息ID
 * 出口参数：无
 * 示    例：MCU发送 "AA3SWTV3.0GET5495FF"
 *          模组返回 "AA3SWTV3.0SUC5409FF"
 * 说    明：这里的处理函数主要用于MCU主动查询网络状态的场景，data参数暂未使用，msgid用于构建响应帧
****************************************************************************************/
void drv_handle_cmd_3(const char *data, const char *msgid)
{
    char frame[32];
    strcpy(frame, global_cmd3_frame);

    /* 修改网络状态：这部分在drv_send_cmd3_push中已经修改，无需再次判断、修改 */

    /* 修改msgID：使用对方发过来的msgID */
    frame[13] = msgid[0];
    frame[14] = msgid[1];

    /* 计算CRC：从AA到msgID（位置0-16） */
    unsigned char crc_result[3];
    uart_protocol_calculate_crc((unsigned char*)frame, 17, crc_result);
    frame[15] = crc_result[0];
    frame[16] = crc_result[1];

    /* 发送响应帧 (发送实际字符长度，避免传输 '\0') */
    liot_uart_write(LIOT_UART_PORT_2, (unsigned char*)frame, (int)strlen(frame));
    liot_trace("[DRV] Sent CMD3 response frame: %s", frame);
}


/****************************************************************************************
 * 名    称：drv_handle_cmd_4
 * 功    能：处理UART发送CMD='4'的状态帧，解析并更新IO状态，反馈ACK
 * 入口参数：data - 状态字段（例如"0001"或"0000"），msgid - 对方发送的消息ID
 * 出口参数：无
 * 示    例：MCU发送 "AA4000031XXFF"
 *          模组返回 "AA400003100FF"
 * 说    明：
 *  - 若在平台命令保护时间内接收到CMD4帧，则自动ACK并忽略处理
 *  - 否则解析data字段，更新IO状态，若状态有变化则触发平台消息
 *  - 最后发送ACK响应帧
 ****************************************************************************************/
void drv_handle_cmd_4(const char *data, const char *msgid)
{
    liot_trace("[DRV] CMD4 data=%s msgid=%s", data, msgid);

    // Step1: 平台2秒保护
    if (drv_cmd4_check_platform_protect_and_ack(msgid)) {
        return;
    }

    // Step2: 解析IO状态
    int io_changed = drv_cmd4_parse_and_update_io(data);

    // Step3: 若有变化则通知平台
    if (io_changed) {
        drv_cmd4_handle_io_changed();
    }

    // Step4: 发送ACK
    drv_cmd4_send_ack(msgid);
}

/*******************************************************************************************************************************************************
 * 名    称：drv_cmd4_check_platform_protect_and_ack
 * 功    能：检查是否处于平台命令2秒保护期内，若是则自动发送ACK并返回1，否则返回0
 * 入口参数：msgid - 对方发送的消息ID，用于构建ACK
 * 说    明：
 *  - 平台命令保护期定义：在平台命令下发，在一段时间内（例如2秒），如果接收到CMD4，则可能冲突，此时已平台优先级为准，自动ACK并忽略UART处理，避免重复执行或者逻辑混乱
 * 返 回 值：1 = 已自动ACK并结束处理
 *          0 = 可以继续执行后续逻辑
 ******************************************************************************************************************************************************/
int drv_cmd4_check_platform_protect_and_ack(const char *msgid)
{
    if (uart2.cmd4_platform_rcv_cnt < TIME_PLATFORM_CMD_PROTECT_2S) {
        unsigned char buf[32];
        int blen = 0;

        liot_trace("[DRV] CMD4 received within 2s of platform command, auto ACK without processing, msgid=%s", msgid);

        uart_protocol_build_ack('4', '0', msgid, buf, &blen);
        liot_uart_write(LIOT_UART_PORT_2, buf, blen);

        return 1;   // 已处理
    }

    return 0;       // 未命中保护期
}

/****************************************************************************************
 * 名    称：drv_cmd4_parse_and_update_io
 * 功    能：解析CMD4的IO状态并更新g_io结构，判断是否有状态变化
 * 入口参数：data - CMD4帧中的IO状态字段（例如"0001"表示IO_OUT_1 ON，"0000"表示全部OFF）
 * 返 回 值：1 = IO状态发生变化
 *          0 = 无变化
 * 说    明：
 *      1. 支持4路输出
 *      2. 仅允许字符 '0' 或 '1'
 *      3. 若格式错误直接返回0
 ***************************************************************************************/
int drv_cmd4_parse_and_update_io(const char *data)
{
    int io_status_changed = 0;

    /* 基本长度校验 */
    if (!data || strlen(data) < 4)
        return 0;

    for (int i = 0; i < 4; i++) {
        char c = data[i];

        /* 字符合法性判断 */
        if (c != '0' && c != '1') {
            liot_trace("UART: IO format error");
            return 0;
        }

        e_OUT_STATUS new_status = map_data_to_io_status(data[i]);

        if (new_status != g_io.out[i].pre_status) {
            g_io.out[i].now_status = new_status;
            g_io.out[i].pre_status = new_status;
            io_status_changed = 1;
        } else {
            g_io.out[i].now_status = new_status;
        }
    }

    return io_status_changed;
}


/****************************************************************************************
 * 名    称：drv_cmd4_handle_io_changed
 * 功    能：IO状态变化后发送平台消息并更新IO输出
 ***************************************************************************************/
void drv_cmd4_handle_io_changed(void)
{
    comm_msg_t msg;
    msg.source = TRIGGER_SOURCE_UART;
    msg.type   = MSG_TYPE_UART_TRIGGER_SWITCH_STATUS_CHANGE;

    LiotOSStatus_t ret = liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0);

    if (ret != LIOT_OSI_SUCCESS) {
        liot_trace("platform_queue full, UART msg dropped, source=%d", msg.source);
    } else {
        liot_trace("UART IO status changed, message sent to platform_queue, source=%d", msg.source);
    }

    update_io_state_from_g_io();
}

/****************************************************************************************
 * 名    称：drv_cmd4_send_ack
 * 功    能：发送CMD4 ACK响应
 ***************************************************************************************/
void drv_cmd4_send_ack(const char *msgid)
{
    unsigned char buf[32];
    int blen = 0;

    uart_protocol_build_ack('4', '0', msgid, buf, &blen);
    liot_uart_write(LIOT_UART_PORT_2, buf, blen);
}

// Define a mapping function for IO status
e_OUT_STATUS map_data_to_io_status(char data_char) {
    switch (data_char) {
        case '0': return enIO_OUT_OFF;
        case '1': return enIO_OUT_ON;
        default: return enIO_OUT_OFF; // Handle unexpected values
    }
}

/****************************************************************************************
 * 名    称：update_io_state_from_g_io
 * 功    能：根据全局g_io结构的状态更新实际IO输出状态
 * 说    明：
 *  - 该函数会根据g_io.out数组中每个IO的now_status来设置对应的IO输出引脚状态
 *  - 该函数可以在IO状态变化后被调用，以确保物理IO输出与g_io结构中的状态保持一致
 ***************************************************************************************/
void update_io_state_from_g_io() {
#if IO_SWITCH_OUTPUT_ENABLE

    #if FOUR_CHANNEL_OUTPUT_ENABLE
    // Update IO_OUT_4
    if (g_io.out[0].now_status == enIO_OUT_ON) {
        IO_OUT_4_H;
    } else {
        IO_OUT_4_L;
    }

    // Update IO_OUT_3
    if (g_io.out[1].now_status == enIO_OUT_ON) {
        IO_OUT_3_H;
    } else {
        IO_OUT_3_L;
    }

    // Update IO_OUT_2
    if (g_io.out[2].now_status == enIO_OUT_ON) {
        IO_OUT_2_H;
    } else {
        IO_OUT_2_L;
    }
    #endif

    // Update IO_OUT_1
    if (g_io.out[3].now_status == enIO_OUT_ON) {
        IO_OUT_1_H;
        #if MAG_LATCH_PULSE_OUTPUT_ENABLE
        IO_OUT_1_ON_H_100MS;       
        #endif
    } else {
        IO_OUT_1_L;
        #if MAG_LATCH_PULSE_OUTPUT_ENABLE
        IO_OUT_1_OFF_H_100MS;
        #endif
    }

#endif
}


/******************************************************************************
 * 名    称：drv_handle_cmd_5
 * 功    能：处理UART发送CMD='5'的状态帧，解析并更新电压、电流、IO状态、故障码等参数，反馈ACK
 * 入口参数：data - 状态字段（例如"V220.0A030.00001NN2"），msgid - 对方发送的消息ID
 * 出口参数：无
 * 示    例：MCU发送 "AA5V220.0A030.00001NN016FF"
 *          模组返回 "AA5V220.0A030.00001NN0200FF"
 * 架构原则：
 * - 差分触发
 * - 无浮点计算
 * - 固定位置解析
 * - 统一事件驱动
 * 修   改：
 ******************************************************************************/
/******************************************************************************************************
 * 名    称：drv_handle_cmd_5
 * 功    能：处理 CMD='5'（电压/电流/故障/IO 状态上报）
 * 入口参数：data - DATA 字段字符串，msgid - 消息 ID
 * 说    明：
 *  - send_ack=1: 字符串协议，需要发送 ACK
 *  - send_ack=0: Hex 协议转换，不发送 ACK
 *****************************************************************************************************/
void drv_handle_cmd_5(const char *data, const char *msgid, int send_ack)
{
    liot_trace("[DRV] CMD5 data=%s msgid=%s send_ack=%d", data, msgid, send_ack);

    /* 基本合法性检查（最小长度 19 字节） */
    if (!data || strlen(data) < 19) {
        liot_trace("[DRV] CMD5 length error");
        if (send_ack) {
            unsigned char buf[32];
            int blen = 0;
            uart_protocol_build_ack('5', '1', msgid, buf, &blen);
            liot_uart_write(LIOT_UART_PORT_2, buf, blen);
        }
        return;
    }

    /* 1. 解析电压和电流（可能触发流量保护） */
    int vol_cur_changed = 0;
    vol_cur_changed |= drv_cmd5_parse_voltage(data);
    vol_cur_changed |= drv_cmd5_parse_current(data);

    /* 2. 检查电压/电流更新次数（3min 内最多 6 次） */
    if (vol_cur_changed) {
        uart2.cmd_vol_cur_update_cnt++;
        if (uart2.cmd_vol_cur_update_cnt > 6) {
            liot_trace("[CMD5] Voltage/Current update count exceeds limit (cnt=%d, max=6/3min)", uart2.cmd_vol_cur_update_cnt);
            vol_cur_changed = 0;  // 超过次数，忽略电压/电流变化
        }
    }

    /* 3. 解析 IO 开关状态（不受流量保护限制） */
    int io_fault_changed = 0;
    io_fault_changed |= drv_cmd5_parse_switch(data);
    io_fault_changed |= drv_cmd5_parse_fault(data);

    /* 4. 若有任意变化（电压/电流/IO/故障），则通知平台任务 */
    if (vol_cur_changed || io_fault_changed) {
        comm_msg_t msg;
        msg.source = TRIGGER_SOURCE_UART;
        msg.type   = MSG_TYPE_UART_TRIGGER_SWITCH_PARAM_UPDATE;

        if (liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0) != LIOT_OSI_SUCCESS){
            liot_trace("platform_queue full, CMD5 msg dropped");
        }
        else{
            liot_trace("CMD5 state changed, notify platform");
            update_io_state_from_g_io();
        }
    }
    else{
        liot_trace("CMD5 state not changed, no notify to platform");
    }

    /* 发送 ACK（仅字符串协议需要） */
    if (send_ack) {
        unsigned char buf[32];
        int blen = 0;
        uart_protocol_build_ack('5', '0', msgid, buf, &blen);
        liot_uart_write(LIOT_UART_PORT_2, buf, blen);
    }
}

// 电压整数提取
int drv_get_voltage_int(const char *v)
{
    return (v[0] - '0') * 100 + (v[1] - '0') * 10 + (v[2] - '0');
}

// 电流整数提取
int drv_get_current_int(const char *a)
{
    return (a[0] - '0') * 100 + (a[1] - '0') * 10 + (a[2] - '0');
}

/******************************************************************************************************
 * 名    称：drv_cmd5_parse_voltage
 * 功    能：解析CMD5的电压数据，判断电压是否合法，并存储电压值
 * 入口参数：data - CMD5的DATA字段，格式示例 "V220.0A030.00001NN2"
 * 出口参数：返回1表示电压发生了异常变化，返回0表示电压未触发变化阈值
 * 说    明：
 *  - 电压格式要求：以'V'开头，后跟5字符表示电压值（例如"220.0"），总长度至少6字符
 *  - 电压变化规则：
 *    1. 电压跳变 >10V：如果当前电压与上一次存储的电压相比，变化超过10V（无论升高还是降低），则认为发生了异常变化
 *    2. 非0 → 0：如果电压从非0值变为0，也认为发生了异常变化
 *    3. 0 → 非0：如果电压从0值变为非0值，也认为发生了异常变化
 * 修    改：26-03-27 电压触发阈值从10V改为5V
 *****************************************************************************************************/
int drv_cmd5_parse_voltage(const char *data)
{
    if (!data || data[0] != 'V')
        return 0;

    /* 格式: VXXX.X */
    if (data[1]<'0'||data[1]>'9' ||
        data[2]<'0'||data[2]>'9' ||
        data[3]<'0'||data[3]>'9' ||
        data[4] != '.' ||
        data[5]<'0'||data[5]>'9')
    {
        liot_trace("UART: Voltage format error");
        return 0;
    }

    /* 读取旧值 */
    int pre_vol = drv_get_voltage_int(cat1.param.voltage);

    /* 更新新值 */
    memcpy(cat1.param.voltage, &data[1], 5);
    cat1.param.voltage[5] = '\0';
    int cur_vol = drv_get_voltage_int(cat1.param.voltage);
    liot_trace("Parsed voltage: %d (string: %s)", cur_vol, cat1.param.voltage);

    /* 判断是否满足变化条件 */
    int should_return_1 = 0;
    
    /* 规则 1：电压跳变 >5V */
    int diff = cur_vol - pre_vol;
    if (diff > 5 || diff < -5) {
        liot_trace("UART: Voltage change >5V (%d->%d)", pre_vol, cur_vol);
        should_return_1 = 1;
    }

    /* 规则 2：非 0 → 0 */
    if (pre_vol != 0 && cur_vol == 0) {
        liot_trace("UART: Voltage drop to 0 (%d->0)", pre_vol);
        should_return_1 = 1;
    }

    /* 规则 3：0 → 非 0 */
    if (pre_vol == 0 && cur_vol != 0) {
        liot_trace("UART: Voltage rise from 0 (%d)", cur_vol);
        should_return_1 = 1;
    }
    
    return should_return_1;
}

/******************************************************************************************************
 * 名    称：drv_cmd5_parse_current
 * 功    能：解析CMD5的电流数据，判断电流是否合法，并存储电流值
 * 入口参数：data - CMD5的DATA字段，格式示例 "V220.0A030.00001NN2"
 * 出口参数：返回1表示电流发生了异常变化，返回0表示电流未触发变化阈值
 * 说    明：
 *  - 电流格式要求：以'A'开头，后跟5字符表示电流值（例如"030.0"），总长度至少12字符（因为前面还有电压部分）
 *  - 电流变化规则：
 *    1. 电流跳变 >5A：如果当前电流与上一次存储的电流相比，变化超过5A（无论升高还是降低），则认为发生了异常变化
 *    2. 非0 → 0：如果电流从非0值变为0，也认为发生了异常变化
 *    3. 0 → 非0：如果电流从0值变为非0值，也认为发生了异常变化
 * 修    改：26-03-27 电流触发阈值从5A改为2A
 *****************************************************************************************************/
int drv_cmd5_parse_current(const char *data)
{
    if (!data || data[6] != 'A')
        return 0;

    /* 格式: AXXX.X */
    if (data[7]<'0'||data[7]>'9' ||
        data[8]<'0'||data[8]>'9' ||
        data[9]<'0'||data[9]>'9' ||
        data[10] != '.' ||
        data[11]<'0'||data[11]>'9')
    {
        liot_trace("UART: Current format error");
        return 0;
    }

    /* 读取旧值 */
    int pre_cur = drv_get_current_int(cat1.param.current);

    /* 更新新值 */
    memcpy(cat1.param.current, &data[7], 5);
    cat1.param.current[5] = '\0';
    int cur_cur = drv_get_current_int(cat1.param.current);
    liot_trace("Parsed current: %d (string: %s)", cur_cur, cat1.param.current);

    /* 判断是否满足变化条件 */
    int should_return_1 = 0;
    
    /* 规则 1：电流跳变 >2A */
    int diff = cur_cur - pre_cur;
    if (diff > 2 || diff < -2) {
        liot_trace("UART: Current change >2A (%d->%d)", pre_cur, cur_cur);
        should_return_1 = 1;
    }

    /* 规则 2：非 0 → 0 */
    if (pre_cur != 0 && cur_cur == 0) {
        liot_trace("UART: Current drop to 0 (%d->0)", pre_cur);
        should_return_1 = 1;
    }

    /* 规则 3：0 → 非 0 */
    if (pre_cur == 0 && cur_cur != 0) {
        liot_trace("UART: Current rise from 0 (0->%d)", cur_cur);
        should_return_1 = 1;
    }

    return should_return_1;
}

/******************************************************************************************************
 * 名    称：drv_cmd5_parse_switch
 * 功    能：解析CMD5的开关数据，判断开关状态是否合法，并存储开关状态
 * 入口参数：data - CMD5的DATA字段，格式示例 "V220.0A030.00001NN2"
 * 出口参数：返回1表示开关状态发生了变化，返回0表示开关状态未发生变化
 * 说    明：
 * - 开关状态格式要求：从第12位开始，连续4字符，每个字符为'0'或'1'，分别表示4路开关的状态（例如"0001"表示第4路开关ON，其他OFF）
 * - 开关状态变化规则：
 *   1. 如果任一路开关的当前状态与上一次存储的状态不同，则认为开关状态发生了变化
 *   2. 如果开关状态格式不合法（例如长度不足，或包含非'0'/'1'字符），则认为数据有误，不更新状态，也不触发变化
 * 修    改：
 ******************************************************************************************************/
int drv_cmd5_parse_switch(const char *data)
{
    int io_status_changed = 0;

    /* 至少保证能取到第15位 */
    if (!data || strlen(data) < 16)
        return 0;

    const char *io_ptr = &data[12];   // IO 从第12位开始
    for (int i = 0; i < 4; i++) {
        char c = io_ptr[i];
        /* 简单合法性判断，只允许 '0' 或 '1' */
        if (c != '0' && c != '1') {
            liot_trace("UART: IO format error");
            return 0;
        }

        e_OUT_STATUS new_status = map_data_to_io_status(c);
        if (new_status != g_io.out[i].pre_status) {
            g_io.out[i].now_status = new_status;
            g_io.out[i].pre_status = new_status;
            io_status_changed = 1;
        } else {
            g_io.out[i].now_status = new_status;
        }
    }
    liot_trace("Parsed IO status: %c%c%c%c", io_ptr[0], io_ptr[1], io_ptr[2], io_ptr[3]);

    return io_status_changed;
}

device_fault_t device_fault_from_char(char c)
{
    switch (c) {
        case DEV_FAULT_NONE:
        case DEV_FAULT_TIMER:
        case DEV_FAULT_UNDERVOLT:
        case DEV_FAULT_OVERVOLT:
        case DEV_FAULT_UNDERCURRENT:
        case DEV_FAULT_OVERCURRENT:
        case DEV_FAULT_NOLOAD:
        case DEV_FAULT_OVERLOAD:
        case DEV_FAULT_LEAKAGE:
        case DEV_FAULT_PHASE_LOSS:
        case DEV_FAULT_PHASE_A:
        case DEV_FAULT_PHASE_B:
        case DEV_FAULT_PHASE_C:
        case DEV_FAULT_SHORT_CIRCUIT:
            return (device_fault_t)c;

        default:
            return DEV_FAULT_NONE; // 默认返回无故障，避免误判
    }
}

/******************************************************************************
 * @brief  根据 fault 获取故障名称字符串 (用于日志打印)
 * @param  fault : 当前设备故障码
 * @return const char* : 对应故障名称字符串
 ******************************************************************************/
const char* drv_get_fault_name(device_fault_t fault)
{
    switch (fault) {
        case DEV_FAULT_NONE:          return "NONE";
        case DEV_FAULT_TIMER:         return "TIMER";
        case DEV_FAULT_UNDERVOLT:     return "UNDERVOLT";
        case DEV_FAULT_OVERVOLT:      return "OVERVOLT";
        case DEV_FAULT_UNDERCURRENT:  return "UNDERCURRENT";
        case DEV_FAULT_OVERCURRENT:   return "OVERCURRENT";
        case DEV_FAULT_NOLOAD:        return "NOLOAD";
        case DEV_FAULT_OVERLOAD:      return "OVERLOAD";
        case DEV_FAULT_LEAKAGE:       return "LEAKAGE";
        case DEV_FAULT_PHASE_LOSS:    return "PHASE_LOSS";
        case DEV_FAULT_PHASE_A:       return "PHASE_A";
        case DEV_FAULT_PHASE_B:       return "PHASE_B";
        case DEV_FAULT_PHASE_C:       return "PHASE_C";
        case DEV_FAULT_SHORT_CIRCUIT: return "SHORT_CIRCUIT";
        default:                      return "UNKNOWN";
    }
}
/******************************************************************************************************
 * 名    称：drv_cmd5_parse_fault
 * 功    能：解析CMD5的故障码数据，判断故障状态是否合法，并存储故障码
 * 入口参数：data - CMD5的DATA字段，格式示例 "V220.0A030.00001NN2"
 * 出口参数：返回1表示故障码发生了变化，返回0表示故障码未发生变化或格式错误
 * 说    明：
 *  - 故障码格式要求：位于第18位（从0开始计数），单个字符表示故障类型
 *  - 故障码变化规则：
 *    1. 如果当前故障码与上一次存储的故障码相同，则不触发变化（返回0）
 *    2. 如果当前故障码与上一次存储的故障码不同，则触发变化（返回1），并更新故障码和报警字符串
 *  - 无效故障码（DEV_FAULT_INVALID）会被视为格式错误，不更新状态
 *  - 故障码通过查表映射为3字符报警字符串（如"OVP"、"OCP"等）
 * 修    改：
 *****************************************************************************************************/
int drv_cmd5_parse_fault(const char *data)
{
    if (!data || strlen(data) < 19)
        return 0;

    device_fault_t pre_fault = cat1.param.dev_fault;

    if (data[18] == DEV_FAULT_INVALID) {
        liot_trace("UART: Fault format error (raw='%c', name='%s')", data[18], drv_get_fault_name(DEV_FAULT_INVALID));
        return 0;
    }
    device_fault_t cur_fault = device_fault_from_char(data[18]);
    
    liot_trace("UART: CMD5 Fault received (raw='%c', name='%s')", data[18], drv_get_fault_name(cur_fault));

    /* 规则 1：未变化不触发 */
    if (cur_fault == pre_fault)
        return 0;

    /* 规则 2：变化才触发 */
    liot_trace("UART: CMD5 Fault changed (pre='%s', cur='%s')", drv_get_fault_name(pre_fault), drv_get_fault_name(cur_fault));

    cat1.param.dev_fault = cur_fault;
    
    /* 查表更新 alarm */
    drv_cmd5_update_alarm(cur_fault);

    return 1;
}

/******************************************************************************
 * @brief  故障码映射表
 * 说明：
 * 1. 所有未定义 fault 默认返回 "NON"
 * 2. 可直接在此扩展新的报警类型
 ******************************************************************************/
const fault_alarm_map_t g_fault_alarm_table[] =
{
    /* 无故障类 */
    { DEV_FAULT_NONE,          "NON" },
    { DEV_FAULT_TIMER,         "TIM" },
    /* 电压类 */
    { DEV_FAULT_UNDERVOLT,     "UVP" },
    { DEV_FAULT_OVERVOLT,      "OVP" },
    /* 电流类 */
    { DEV_FAULT_UNDERCURRENT,  "UCP" },
    { DEV_FAULT_OVERCURRENT,   "OCP" },
    { DEV_FAULT_NOLOAD,        "NLD" },
    { DEV_FAULT_OVERLOAD,      "OLD" },
    { DEV_FAULT_LEAKAGE,       "LKG" },
    /* 相位类 */
    { DEV_FAULT_PHASE_LOSS,    "PHL" },
    { DEV_FAULT_PHASE_A,       "PHA" },
    { DEV_FAULT_PHASE_B,       "PHB" },
    { DEV_FAULT_PHASE_C,       "PHC" },
    /* 特殊类 */
    { DEV_FAULT_SHORT_CIRCUIT, "SCP" },
};

/******************************************************************************
 * @brief  根据 fault 查找 alarm 字符串
 * @param  fault : 当前设备故障码
 * @return const char* : 对应报警字符串
 *
 * 规则：
 * 1. 查表匹配
 * 2. 未匹配返回 "NON"
 ******************************************************************************/
const char* drv_cmd5_get_alarm_from_fault(device_fault_t fault)
{
    for (unsigned int i = 0;
         i < sizeof(g_fault_alarm_table)/sizeof(g_fault_alarm_table[0]);
         i++)
    {
        if (g_fault_alarm_table[i].fault == fault)
        {
            return g_fault_alarm_table[i].alarm;
        }
    }

    /* 未匹配默认无报警 */
    return "NON";
}

/******************************************************************************
 * @brief  更新 cat1.param.alarm 字段
 ******************************************************************************/
void drv_cmd5_update_alarm(device_fault_t fault)
{
    liot_trace("Updating alarm for fault code: %c", (char)fault);
    const char *alarm_str = drv_cmd5_get_alarm_from_fault(fault);

    strncpy(cat1.param.alarm, alarm_str, ALARM_STR_LEN);
    cat1.param.alarm[ALARM_STR_LEN] = '\0';

    liot_trace("UART: Alarm updated -> %s", cat1.param.alarm);
}

void drv_handle_cmd_7(const char *data, const char *msgid, int send_ack)
{
    liot_trace("[DRV] CMD7 data=%s msgid=%s", data, msgid);

    /* Step1: 平台 2 秒保护 */
    if (drv_cmd7_check_platform_protect_and_ack(msgid)) {
        return;
    }

    /* Step2: 解析工作模式和电机状态 */
    int cmd_valid = drv_cmd7_parse_and_validate(data);
    if (!cmd_valid) {
        liot_trace("[DRV] CMD7 command invalid");
        if (send_ack) {
            drv_cmd7_send_ack('1', msgid);
        }
        return;
    }

    /* Step3: 执行电机控制命令 */
    int motor_state_changed = drv_cmd7_execute_command(data);

    /* Step4: 如果状态变化，处理后续逻辑 */
    if (motor_state_changed) {
        drv_cmd7_handle_motor_changed();
    }

    /* Step5: 发送 ACK */
    if (send_ack) {
        drv_cmd7_send_ack('0', msgid);
    }
}

/******************************************************************************************************
 * 名    称：drv_cmd7_check_platform_protect_and_ack
 * 功    能：检查是否处于平台命令 2 秒保护期内，若是则自动 ACK 并返回 1，否则返回 0
 * 入口参数：msgid - 对方发送的消息 ID，用于构建 ACK
 * 说    明：
 *  - 平台命令保护期定义：在平台命令下发，在一段时间内（例如 2 秒），如果接收到 CMD7 帧，则可能冲突，此时已平台优先级为准，自动 ACK 并忽略 UART 处理，避免重复执行或者逻辑混乱
 * 返 回 值：1 = 已自动 ACK 并结束处理
 *          0 = 可以继续执行后续逻辑
 * 修    改：26-04-07 适配正反转MCU返回相同模组推送数据，导致HEX推送String字符串问题
 *****************************************************************************************************/
int drv_cmd7_check_platform_protect_and_ack(const char *msgid)
{
    if (uart2.cmd7_platform_rcv_cnt < TIME_PLATFORM_CMD_PROTECT_2S) {
        // unsigned char buf[32];
        // int blen = 0;

        // liot_trace("[DRV] CMD7 received within 2s of platform command, auto ACK without processing, msgid=%s", msgid);

        // uart_protocol_build_ack('7', '0', msgid, buf, &blen);
        // liot_uart_write(LIOT_UART_PORT_2, buf, blen);

        return 1;   // 已处理
    }

    return 0;       // 未命中保护期
}

/******************************************************************************************************
 * 名    称：drv_cmd7_parse_and_validate
 * 功    能：解析 CMD7 的电机控制命令并验证合法性
 * 入口参数：data - CMD7 的 DATA 字段，格式为 2 字符组合：
 *                第 1 位：工作模式 (J=点动模式 / L=联动模式)
 *                第 2 位：电机状态 (F=正转 / R=反转 / T=停止)
 *                示例："JF"=点动正转，"JR"=点动反转，"JT"=点动停止
 *                      "LF"=联动正转，"LR"=联动反转，"LT"=联动停止
 * 出口参数：返回 1 表示命令合法，返回 0 表示命令非法
 * 说    明：
 *  - 工作模式：J=Jog(点动模式) / L=Linkage(联动模式)
 *  - 电机状态：F=Forward(正转) / R=Reverse(反转) / T=Stop(停止)
 *  - 必须同时指定工作模式和电机状态，共 2 字符
 * 修    改：
 *****************************************************************************************************/
int drv_cmd7_parse_and_validate(const char *data)
{
    if (!data || strlen(data) < 2) {
        liot_trace("[DRV] CMD7 data length must be 2 chars");
        return 0;
    }

    char mode = data[0];  /* 工作模式 */
    char state = data[1]; /* 电机状态 */
    
    /* 检查工作模式合法性 */
    if (mode != 'J' && mode != 'L') {
        liot_trace("[DRV] CMD7 invalid mode: %c (expected J or L)", mode);
        return 0;
    }

    /* 检查电机状态合法性 */
    if (state != 'F' && state != 'R' && state != 'T') {
        liot_trace("[DRV] CMD7 invalid state: %c (expected F/R/T)", state);
        return 0;
    }

    liot_trace("[DRV] CMD7 command validated: mode=%c state=%c", mode, state);
    return 1;
}

/******************************************************************************************************
 * 名    称：drv_cmd7_execute_command
 * 功    能：执行电机控制命令，根据工作模式和电机状态组合更新电机状态
 * 入口参数：data - CMD7 的 DATA 字段，2 字符组合命令
 *                第 1 位：工作模式 (J=点动 / L=联动)
 *                第 2 位：电机状态 (F=正转 / R=反转 / T=停止)
 * 出口参数：返回 1 表示电机状态发生变化，返回 0 表示无变化
 * 说    明：
 *  - 根据工作模式和电机状态的组合，设置对应的电机状态
 *  - 点动模式 (J)：
 *      F=正转 (MODE_FORWARD_JOG) / R=反转 (MODE_REVERSE_JOG) / T=停止 (MODE_IDLE_JOG)
 *  - 联动模式 (L)：
 *      F=正转 (MODE_FORWARD_HOLD) / R=反转 (MODE_REVERSE_HOLD) / T=停止 (MODE_IDLE_HOLD)
 *  - 更新 preState 和 currentState
 *  - 如果状态发生变化，发送消息到平台队列通知状态变化
 *  - 更新实际 IO 输出
 * 修    改：
 *****************************************************************************************************/
int drv_cmd7_execute_command(const char *data)
{
    int motor_state_changed = 0;

    /* 更新电机状态 */
    #if PRODUCT_MOTOR
    char mode = data[0];   /* 工作模式：J=点动 / L=联动 */
    char state = data[1];  /* 电机状态：F=正转 / R=反转 / T=停止 */
    motor_state_t new_state;
    
    /* 根据工作模式和电机状态组合确定新状态 */
    if (mode == 'J') {
        /* 点动模式 */
        if (state == 'F') {
            new_state = MODE_FORWARD_JOG;
            liot_trace("[DRV] CMD7: Jog Forward");
        } else if (state == 'R') {
            new_state = MODE_REVERSE_JOG;
            liot_trace("[DRV] CMD7: Jog Reverse");
        } else if (state == 'T') {
            new_state = MODE_IDLE_JOG;
            liot_trace("[DRV] CMD7: Jog Stop");
        } else {
            new_state = motor.currentState;  // 无效状态，保持不变
        }
    } else if (mode == 'L') {
        /* 联动模式 */
        if (state == 'F') {
            new_state = MODE_FORWARD_HOLD;
            liot_trace("[DRV] CMD7: Linkage Forward");
        } else if (state == 'R') {
            new_state = MODE_REVERSE_HOLD;
            liot_trace("[DRV] CMD7: Linkage Reverse");
        } else if (state == 'T') {
            new_state = MODE_IDLE_HOLD;
            liot_trace("[DRV] CMD7: Linkage Stop");
        } else {
            new_state = motor.currentState;  // 无效状态，保持不变
        }
    } else {
        new_state = motor.currentState;  // 无效模式，保持不变
    }
    
    /* 判断状态是否发生变化（与当前状态比较） */
    if (new_state != motor.currentState) {
        motor_state_changed = 1;

        motor.preState = new_state;
        motor.currentState = new_state;
        
        liot_trace("[DRV] CMD7: Motor state changed");
    }
    /* 如果状态未变化，不需要任何操作（currentState 已经是 new_state） */
    #endif

    return motor_state_changed;
}

/******************************************************************************************************
 * 名    称：drv_cmd7_handle_motor_changed
 * 功    能：处理电机状态变化后的逻辑（通知平台 + 更新 IO 输出）
 * 说    明：
 *  - 发送消息到平台队列通知状态变化
 *  - 更新实际 IO 输出
 * 修    改：
 *****************************************************************************************************/
void drv_cmd7_handle_motor_changed()
{
#if PRODUCT_MOTOR
    comm_msg_t msg;
    msg.source = TRIGGER_SOURCE_UART;
    msg.type   = MSG_TYPE_UART_TRIGGER_MOTOR_STATUS_CHANGE;

    if (liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0) != LIOT_OSI_SUCCESS) {
        liot_trace("[DRV] CMD7: platform_queue full, message dropped");
    } else {
        liot_trace("[DRV] CMD7: state changed, notify platform");
    }

    /* 更新实际 IO 输出 */
    update_motor_io_state();
#endif
}

/******************************************************************************************************
 * 名    称：update_motor_io_state
 * 功    能：根据 motor.currentState 更新电机控制 IO 输出
 * 说    明：
 *  - 根据电机正反转状态，设置对应的 IO 引脚和 LED 指示灯
 *  - 正转：MOTOR_FORWARD_OUT + MOTOR_FORWARD_LED
 *  - 反转：MOTOR_REVERSE_OUT + MOTOR_REVERSE_LED
 *  - 停止：所有 IO 和 LED 都关闭
 *  - 防止正反转信号同时为真的保护逻辑
 * 修    改：
 *****************************************************************************************************/
void update_motor_io_state()
{
#if IO_MOTOR_OUTPUT_ENABLE
    int forward_active = 0;  // 正转信号是否激活
    int reverse_active = 0;  // 反转信号是否激活
    
    /* 判断正反转状态 */
    switch (motor.currentState) {
        case MODE_FORWARD_HOLD:
        case MODE_FORWARD_JOG:
            forward_active = 1;
            break;
            
        case MODE_REVERSE_HOLD:
        case MODE_REVERSE_JOG:
            reverse_active = 1;
            break;
            
        case MODE_IDLE_HOLD:
        case MODE_IDLE_JOG:
        default:
            forward_active = 0;
            reverse_active = 0;
            break;
    }
    
    /* 保护逻辑：防止正反转信号同时为真 */
    if (forward_active && reverse_active) {
        liot_trace("[DRV] MOTOR: Protection! Both forward and reverse active, ignoring");
        return;
    }
    
    /* 设置正转输出 */
    if (forward_active) {
        MOTOR_FORWARD_OUT_H;
        MOTOR_FORWARD_LED_OUT_H;
    } else {
        MOTOR_FORWARD_OUT_L;
        MOTOR_FORWARD_LED_OUT_L;
    }
    
    /* 设置反转输出 */
    if (reverse_active) {
        MOTOR_REVERSE_OUT_H;
        MOTOR_REVERSE_LED_OUT_H;
    } else {
        MOTOR_REVERSE_OUT_L;
        MOTOR_REVERSE_LED_OUT_L;
    }
    
    liot_trace("[DRV] MOTOR: IO updated - F=%d, R=%d", forward_active, reverse_active);
#endif
}

/******************************************************************************************************
 * 名    称：drv_cmd7_send_ack
 * 功    能：发送 CMD7 ACK 响应
 * 入口参数：result - 结果码字符 ('0' 成功，'1' 失败)
 *          msgid - 原帧中的消息 ID
 * 出口参数：无
 * 修    改：
 *****************************************************************************************************/
void drv_cmd7_send_ack(char result, const char *msgid)
{
    unsigned char buf[32];
    int blen = 0;

    uart_protocol_build_ack('7', result, msgid, buf, &blen);
    liot_uart_write(LIOT_UART_PORT_2, buf, blen);
    
    if (result == '0') {
        liot_trace("[DRV] CMD7 ACK sent: SUCCESS, msgid=%s", msgid);
    } else {
        liot_trace("[DRV] CMD7 ACK sent: FAIL, msgid=%s", msgid);
    }
}


/******************************************************************************************************
 * 名    称：drv_handle_cmd_8
 * 功    能：处理 CMD='8'（电机扩展参数：电压/电流/故障/电机控制）
 * 入口参数：data - DATA 字段字符串，msgid - 消息 ID，send_ack - 是否发送 ACK
 * 说    明：
 *  - send_ack=1: 字符串协议，需要发送 ACK
 *  - send_ack=0: Hex 协议转换，不发送 ACK
 *****************************************************************************************************/
void drv_handle_cmd_8(const char *data, const char *msgid, int send_ack)
{
    liot_trace("[DRV] CMD8 data=%s msgid=%s send_ack=%d", data, msgid, send_ack);
    
    /* 1. 平台 2 秒保护检查 */
    if (drv_cmd8_check_platform_protect_and_ack(msgid)) {
        return;
    }
    
    /* 2. 解析工作模式和电机状态（合法性检查） */
    int cmd_valid = drv_cmd8_parse_and_validate(data);
    if (!cmd_valid) {
        liot_trace("[DRV] CMD8 command invalid");
        if (send_ack) {
            drv_cmd8_send_ack('1', msgid);
        }
        return;
    }
    
    /* 3. 解析电压和电流（可能触发流量保护） */
    int vol_cur_changed = 0;
    vol_cur_changed |= drv_cmd8_parse_voltage(data);
    vol_cur_changed |= drv_cmd8_parse_current(data);
    
    /* 4. 检查电压/电流更新次数（3min 内最多 6 次） */
    if (vol_cur_changed) {
        uart2.cmd_vol_cur_update_cnt++;
        if (uart2.cmd_vol_cur_update_cnt > 6) {
            liot_trace("[CMD8] Voltage/Current update count exceeds limit (cnt=%d, max=6/3min)", 
                       uart2.cmd_vol_cur_update_cnt);
            vol_cur_changed = 0;  // 超过次数，忽略电压/电流变化
        }
    }
    
    /* 5. 解析故障状态（不受流量保护限制） */
    int fault_changed = drv_cmd8_parse_fault(data);
    
    /* 6. 执行电机控制命令（不受流量保护限制） */
    int motor_state_changed = drv_cmd8_execute_command(data);
    
    /* 7. 若有任意变化（电压/电流/故障/电机），则通知平台任务 */
    if (vol_cur_changed || fault_changed || motor_state_changed) {
        drv_cmd8_handle_changed();
    }
    else {
        liot_trace("CMD8 state not changed, no notify to platform");
    }
    
    /* 8. 发送 ACK（仅字符串协议需要） */
    if (send_ack) {
        drv_cmd8_send_ack('0', msgid);
    }
}

/******************************************************************************************************
 * 名    称：drv_cmd8_check_platform_protect_and_ack
 * 功    能：检查是否处于平台命令 2 秒保护期内，若是则自动 ACK 并返回 1，否则返回 0
 * 入口参数：msgid - 对方发送的消息 ID，用于构建 ACK
 * 说    明：
 *  - 平台命令保护期定义：在平台命令下发，在一段时间内（例如 2 秒），如果接收到 CMD8 帧，则可能冲突，此时已平台优先级为准，自动 ACK 并忽略 UART 处理，避免重复执行或者逻辑混乱
 * 返 回 值：1 = 已自动 ACK 并结束处理
 *          0 = 可以继续执行后续逻辑
 * 修    改：
 *****************************************************************************************************/
int drv_cmd8_check_platform_protect_and_ack(const char *msgid)
{
    if (uart2.cmd7_platform_rcv_cnt < TIME_PLATFORM_CMD_PROTECT_2S) {
        unsigned char buf[32];
        int blen = 0;

        liot_trace("[DRV] CMD8 received within 2s of platform command, auto ACK without processing, msgid=%s", msgid);

        uart_protocol_build_ack('8', '0', msgid, buf, &blen);
        liot_uart_write(LIOT_UART_PORT_2, buf, blen);

        return 1;   // 已处理
    }

    return 0;       // 未命中保护期
}

/******************************************************************************************************
 * 名    称：drv_cmd8_parse_and_validate
 * 功    能：解析 CMD8 的电机控制命令并验证合法性
 * 入口参数：data - CMD8 的 DATA 字段，格式为 2 字符组合：
 *                第 1 位：工作模式 (J=点动模式 / L=联动模式)
 *                第 2 位：电机状态 (F=正转 / R=反转 / T=停止)
 *                示例："JF"=点动正转，"JR"=点动反转，"JT"=点动停止
 *                      "LF"=联动正转，"LR"=联动反转，"LT"=联动停止
 * 出口参数：返回 1 表示命令合法，返回 0 表示命令非法
 * 说    明：
 *  - 工作模式：J=Jog(点动模式) / L=Linkage(联动模式)
 *  - 电机状态：F=Forward(正转) / R=Reverse(反转) / T=Stop(停止)
 *  - 必须同时指定工作模式和电机状态，共 2 字符
 * 修    改：
 *****************************************************************************************************/
int drv_cmd8_parse_and_validate(const char *data)
{
    if (!data || strlen(data) < 2) {
        liot_trace("[DRV] CMD8 data length must be 2 chars");
        return 0;
    }

    char mode = data[0];  /* 工作模式 */
    char state = data[1]; /* 电机状态 */
    
    /* 检查工作模式合法性 */
    if (mode != 'J' && mode != 'L') {
        liot_trace("[DRV] CMD8 invalid mode: %c (expected J or L)", mode);
        return 0;
    }

    /* 检查电机状态合法性 */
    if (state != 'F' && state != 'R' && state != 'T') {
        liot_trace("[DRV] CMD8 invalid state: %c (expected F/R/T)", state);
        return 0;
    }

    liot_trace("[DRV] CMD8 command validated: mode=%c state=%c", mode, state);
    return 1;
}

/******************************************************************************************************
 * 名    称：drv_cmd8_parse_voltage
 * 功    能：解析 CMD8 的电压数据，判断电压是否合法，并存储电压值
 * 入口参数：data - CMD8 的 DATA 字段，格式示例 "JTV220.0A030.00NN016XX"
 * 出口参数：返回 1 表示电压发生了异常变化，返回 0 表示电压未触发变化阈值
 * 说    明：
 *  - 电压格式要求：以'V'开头，后跟 5 字符表示电压值（例如"220.0"），总长度至少 6 字符（从第 2 位开始）
 *  - 电压变化规则：
 *    1. 电压跳变 >10V：如果当前电压与上一次存储的电压相比，变化超过 10V（无论升高还是降低），则认为发生了异常变化
 *    2. 非 0 → 0：如果电压从非 0 值变为 0，也认为发生了异常变化
 *    3. 0 → 非 0：如果电压从 0 值变为非 0 值，也认为发生了异常变化
 * 修    改：
 *****************************************************************************************************/
int drv_cmd8_parse_voltage(const char *data)
{
    if (!data || data[2] != 'V')
        return 0;

    /* 格式：VXXX.X */
    if (data[3]<'0'||data[3]>'9' ||
        data[4]<'0'||data[4]>'9' ||
        data[5]<'0'||data[5]>'9' ||
        data[6] != '.' ||
        data[7]<'0'||data[7]>'9')
    {
        liot_trace("UART: Voltage format error");
        return 0;
    }

    /* 读取旧值 */
    int pre_vol = drv_get_voltage_int(cat1.param.voltage);

    /* 更新新值 */
    memcpy(cat1.param.voltage, &data[3], 5);
    cat1.param.voltage[5] = '\0';
    int cur_vol = drv_get_voltage_int(cat1.param.voltage);
    liot_trace("Parsed voltage: %d (string: %s)", cur_vol, cat1.param.voltage);

    /* 判断是否满足变化条件 */
    int should_return_1 = 0;
    
    /* 规则 1：电压跳变 >10V */
    int diff = cur_vol - pre_vol;
    if (diff > 10 || diff < -10) {
        liot_trace("UART: Voltage change >10V (%d->%d)", pre_vol, cur_vol);
        should_return_1 = 1;
    }

    /* 规则 2：非 0 → 0 */
    if (pre_vol != 0 && cur_vol == 0) {
        liot_trace("UART: Voltage drop to 0 (%d->0)", pre_vol);
        should_return_1 = 1;
    }

    /* 规则 3：0 → 非 0 */
    if (pre_vol == 0 && cur_vol != 0) {
        liot_trace("UART: Voltage rise from 0 (%d)", cur_vol);
        should_return_1 = 1;
    }
    
    /* 如果需要返回 1，先检查计数 */
    if (should_return_1 == 1) {
        uart2.cmd_vol_cur_update_cnt++;
        if (uart2.cmd_vol_cur_update_cnt > 6) {
            liot_trace("UART: CMD8 voltage/current update count exceeds limit");
            should_return_1 = 0;  // 超过次数，清 0
        }
    }
    
    return should_return_1;
}

/******************************************************************************************************
 * 名    称：drv_cmd8_parse_current
 * 功    能：解析 CMD8 的电流数据，判断电流是否合法，并存储电流值
 * 入口参数：data - CMD8 的 DATA 字段，格式示例 "JTV220.0A030.00NN016XX"
 * 出口参数：返回 1 表示电流发生了异常变化，返回 0 表示电流未触发变化阈值
 * 说    明：
 *  - 电流格式要求：以'A'开头，后跟 5 字符表示电流值（例如"030.0"），总长度至少 12 字符（因为前面还有电压部分）
 *  - 电流变化规则：
 *    1. 电流跳变 >5A：如果当前电流与上一次存储的电流相比，变化超过 5A（无论升高还是降低），则认为发生了异常变化
 *    2. 非 0 → 0：如果电流从非 0 值变为 0，也认为发生了异常变化
 *    3. 0 → 非 0：如果电流从 0 值变为非 0 值，也认为发生了异常变化
 * 修    改：
 *****************************************************************************************************/
int drv_cmd8_parse_current(const char *data)
{
    if (!data || data[8] != 'A')
        return 0;

    /* 格式：AXXX.X */
    if (data[9]<'0'||data[9]>'9' ||
        data[10]<'0'||data[10]>'9' ||
        data[11]<'0'||data[11]>'9' ||
        data[12] != '.' ||
        data[13]<'0'||data[13]>'9')
    {
        liot_trace("UART: Current format error");
        return 0;
    }

    /* 读取旧值 */
    int pre_cur = drv_get_current_int(cat1.param.current);

    /* 更新新值 */
    memcpy(cat1.param.current, &data[9], 5);
    cat1.param.current[5] = '\0';
    int cur_cur = drv_get_current_int(cat1.param.current);
    liot_trace("Parsed current: %d (string: %s)", cur_cur, cat1.param.current);

    /* 判断是否满足变化条件 */
    int should_return_1 = 0;
    
    /* 规则 1：电流跳变 >5A */
    int diff = cur_cur - pre_cur;
    if (diff > 5 || diff < -5) {
        liot_trace("UART: Current change >5A (%d->%d)", pre_cur, cur_cur);
        should_return_1 = 1;
    }

    /* 规则 2：非 0 → 0 */
    if (pre_cur != 0 && cur_cur == 0) {
        liot_trace("UART: Current drop to 0 (%d->0)", pre_cur);
        should_return_1 = 1;
    }

    /* 规则 3：0 → 非 0 */
    if (pre_cur == 0 && cur_cur != 0) {
        liot_trace("UART: Current rise from 0 (0->%d)", cur_cur);
        should_return_1 = 1;
    }
    
    /* 如果需要返回 1，先检查计数 */
    if (should_return_1 == 1) {
        uart2.cmd_vol_cur_update_cnt++;
        if (uart2.cmd_vol_cur_update_cnt > 6) {
            liot_trace("UART: CMD8 voltage/current update count exceeds limit");
            should_return_1 = 0;  // 超过次数，清 0
        }
    }
    
    return should_return_1;
}

/******************************************************************************************************
 * 名    称：drv_cmd8_parse_fault
 * 功    能：解析 CMD8 的故障数据，判断故障是否合法，并存储故障值
 * 入口参数：data - CMD8 的 DATA 字段，格式示例 "JTV220.0A030.0NN0"
 * 出口参数：返回 1 表示故障发生了变化，返回 0 表示故障未发生变化
 * 说    明：
 *  - 故障码格式要求：从第 17 位开始，1 字符，表示故障码（'0'-'9','A'-'D'）
 *  - 故障变化规则：
 *    1. 如果故障码与上一次存储的故障码不同，则认为故障发生了变化
 *    2. 如果故障码格式不合法（例如不在有效范围内），则认为数据有误，不更新状态，也不触发变化
 * 修    改：
 *****************************************************************************************************/
int drv_cmd8_parse_fault(const char *data)
{
    if (!data || strlen(data) < 17)
        return 0;

    device_fault_t pre_fault = cat1.param.dev_fault;

    if (data[16] == DEV_FAULT_INVALID) {
        liot_trace("UART: CMD8 Fault format error (raw='%c', name='%s')", data[16], drv_get_fault_name(DEV_FAULT_INVALID));
        return 0;
    }
    device_fault_t cur_fault = device_fault_from_char(data[16]);
    
    liot_trace("UART: CMD8 Fault received (raw='%c', name='%s')", data[16], drv_get_fault_name(cur_fault));

    /* 规则 1：未变化不触发 */
    if (cur_fault == pre_fault)
        return 0;

    /* 规则 2：变化才触发 */
    liot_trace("UART: CMD8 Fault changed (pre='%s', cur='%s')", drv_get_fault_name(pre_fault), drv_get_fault_name(cur_fault));

    cat1.param.dev_fault = cur_fault;

    /* 查表更新 alarm */
    drv_cmd5_update_alarm(cur_fault);

    return 1;
}

/******************************************************************************************************
 * 名    称：drv_cmd8_execute_command
 * 功    能：执行电机控制命令，根据工作模式和电机状态组合更新电机状态
 * 入口参数：data - CMD8 的 DATA 字段，2 字符组合命令
 *                第 1 位：工作模式 (J=点动 / L=联动)
 *                第 2 位：电机状态 (F=正转 / R=反转 / T=停止)
 * 出口参数：返回 1 表示电机状态发生变化，返回 0 表示无变化
 * 说    明：
 *  - 根据工作模式和电机状态的组合，设置对应的电机状态
 *  - 点动模式 (J)：
 *      F=正转 (MODE_FORWARD_JOG) / R=反转 (MODE_REVERSE_JOG) / T=停止 (MODE_IDLE_JOG)
 *  - 联动模式 (L)：
 *      F=正转 (MODE_FORWARD_HOLD) / R=反转 (MODE_REVERSE_HOLD) / T=停止 (MODE_IDLE_HOLD)
 *  - 更新 preState 和 currentState
 *  - 如果状态发生变化，返回 1
 * 修    改：
 *****************************************************************************************************/
int drv_cmd8_execute_command(const char *data)
{
    int motor_state_changed = 0;

    /* 更新电机状态 */
    #if PRODUCT_MOTOR
    char mode = data[0];   /* 工作模式：J=点动 / L=联动 */
    char state = data[1];  /* 电机状态：F=正转 / R=反转 / T=停止 */
    motor_state_t new_state;
    
    /* 根据工作模式和电机状态组合确定新状态 */
    if (mode == 'J') {
        /* 点动模式 */
        if (state == 'F') {
            new_state = MODE_FORWARD_JOG;
            liot_trace("[DRV] CMD8: Jog Forward");
        } else if (state == 'R') {
            new_state = MODE_REVERSE_JOG;
            liot_trace("[DRV] CMD8: Jog Reverse");
        } else if (state == 'T') {
            new_state = MODE_IDLE_JOG;
            liot_trace("[DRV] CMD8: Jog Stop");
        } else {
            new_state = motor.currentState;  // 无效状态，保持不变
        }
    } else if (mode == 'L') {
        /* 联动模式 */
        if (state == 'F') {
            new_state = MODE_FORWARD_HOLD;
            liot_trace("[DRV] CMD8: Linkage Forward");
        } else if (state == 'R') {
            new_state = MODE_REVERSE_HOLD;
            liot_trace("[DRV] CMD8: Linkage Reverse");
        } else if (state == 'T') {
            new_state = MODE_IDLE_HOLD;
            liot_trace("[DRV] CMD8: Linkage Stop");
        } else {
            new_state = motor.currentState;  // 无效状态，保持不变
        }
    } else {
        new_state = motor.currentState;  // 无效模式，保持不变
    }
    
    /* 判断状态是否发生变化（与当前状态比较） */
    if (new_state != motor.currentState) {
        motor_state_changed = 1;

        motor.preState = new_state;
        motor.currentState = new_state;
        
        liot_trace("[DRV] CMD8: Motor state changed");
    }
    /* 如果状态未变化，不需要任何操作（currentState 已经是 new_state） */
    #endif

    return motor_state_changed;
}

/******************************************************************************************************
 * 名    称：drv_cmd8_handle_changed
 * 功    能：处理 CMD8 状态变化后的逻辑（通知平台 + 更新 IO 输出）
 * 说    明：
 *  - 发送消息到平台队列通知状态变化
 *  - 更新实际 IO 输出
 * 修    改：
 *****************************************************************************************************/
void drv_cmd8_handle_changed()
{
#if PRODUCT_MOTOR
    comm_msg_t msg;
    msg.source = TRIGGER_SOURCE_UART;
    msg.type   = MSG_TYPE_UART_TRIGGER_MOTOR_PARAM_UPDATE;

    if (liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0) != LIOT_OSI_SUCCESS) {
        liot_trace("[DRV] CMD8: platform_queue full, message dropped");
    } else {
        liot_trace("[DRV] CMD8: state changed, notify platform");
    }

    /* 更新实际 IO 输出 */
    update_motor_io_state();
#endif
}

/******************************************************************************************************
 * 名    称：drv_cmd8_send_ack
 * 功    能：发送 CMD8 ACK 响应
 * 入口参数：result - 结果码字符 ('0' 成功，'1' 失败)
 *          msgid - 原帧中的消息 ID
 * 出口参数：无
 * 修    改：
 *****************************************************************************************************/
void drv_cmd8_send_ack(char result, const char *msgid)
{
    unsigned char buf[32];
    int blen = 0;

    uart_protocol_build_ack('8', result, msgid, buf, &blen);
    liot_uart_write(LIOT_UART_PORT_2, buf, blen);
    
    if (result == '0') {
        liot_trace("[DRV] CMD8 ACK sent: SUCCESS, msgid=%s", msgid);
    } else {
        liot_trace("[DRV] CMD8 ACK sent: FAIL, msgid=%s", msgid);
    }
}


void drv_handle_cmd_9(const char *data, const char *msgid)
{
    /* CMD='9' 管理设置 */
    liot_trace("[DRV] CMD9 data=%s msgid=%s", data, msgid);

    unsigned char buf[32]; 
    int blen = 0; 
    
    if (strncmp(data, "RST", 3) == 0) {     // 匹配成功：str 的前 3 个字符是 "RST"
        liot_trace("[DRV] Received management command: RST");
        // 反馈MCU已收到RST命令
        uart_protocol_build_ack('9', '0', msgid, buf, &blen);
        liot_uart_write(LIOT_UART_PORT_2, buf, blen);
        // 执行重启操作
        FlashWrite_reboot(1);                                                       // 更新Flash，准备复位
        liot_trace("[DRV] FlashWrite_reboot called, system will reboot shortly");
        liot_rtos_task_sleep_s(1);                                                  // 运行时信息不全，这里加入延时
        ResetECSystemReset();
        while(1);                           // 理论上系统会重启，不应该执行到这里
    }

    uart_protocol_build_ack('9', '0', msgid, buf, &blen);
    liot_uart_write(LIOT_UART_PORT_2, buf, blen);
}


void drv_handle_cmd_F(const char *data, const char *msgid)
{
    /* CMD='F' 反馈帧：解析 SRC_CMD 与 RESULT */
    if (strlen(data) >= 2) {
        char src_cmd = data[0]; 
        char result = data[1];
        liot_trace("[DRV] ACK for SRC_CMD=%c RESULT=%c msgid=%s", src_cmd, result, msgid);
        
        // 判断是否为'9'命令的反馈
        if (src_cmd == '9') {
            // 检查反馈是否合法
            if (drv_send_cmd9_push_ack_check()) {
                uart2.isMemStateFeedback = 1;           // 收到合法反馈，设置标志位
                liot_trace("-------> UART2 MEM state feedback received <-------");
            }
        }
        // 判断是否为'4'命令的反馈
        else if (src_cmd == '4') {
            // 检查反馈是否合法
            #if UART_PROTOCOL_FORMAT_ASCII
            drv_send_cmd4_push_ack_check_str();
            #elif UART_PROTOCOL_FORMAT_HEX
            drv_send_cmd4_push_ack_check_hex();
            #endif
                uart2.isIoSwitchFeedback = 1;           // 收到合法反馈，设置标志位
                liot_trace("-------> uart2.isIoSwitchFeedback set to 1 <-------");
        }
        // 判断是否为'7'命令的反馈
        else if (src_cmd == '7') {
            // 检查反馈是否合法
            if (drv_send_cmd7_push_ack_check()) {
                uart2.isIoSwitchFeedback = 1;           // 收到合法反馈，设置标志位
                liot_trace("-------> UART2 Motor control feedback received <-------");
            }
        }
    } else {
        liot_trace("[DRV] Malformed F frame data=%s msgid=%s", data, msgid);
    }
}

/* 根据 CMD 调度到具体处理函数 */
void uart_receive_execute_command(void)
{
    char cmd = 0;
    char data[UART_PROTOCOL_DATA_OUT_SIZE_64] = {0};
    char msgid[3] = {0};

    uart_protocol_parse_fields(uart2.rcvBuff, uart2.receivedLength, &cmd, data, msgid);
    liot_trace("Parsed CMD=%c, data=%s, msgid=%s", cmd, data, msgid);

    if (cmd == 0) return;
    switch (cmd) {
        case '3': drv_handle_cmd_3(data, msgid); break;
        case '4': drv_handle_cmd_4(data, msgid); break;
        case '5': drv_handle_cmd_5(data, msgid, 1); break;  /* 字符串协议，send_ack=1 */
        case '7': drv_handle_cmd_7(data, msgid, 1); break;  /* ASCII 协议，发送 ACK */
        case '8': drv_handle_cmd_8(data, msgid, 1); break;  /* ASCII 协议，发送 ACK */
        case '9': drv_handle_cmd_9(data, msgid); break;
        case 'F': drv_handle_cmd_F(data, msgid); break;
        default:
            liot_trace("Unknown CMD=%c data=%s msgid=%s", cmd, data, msgid);
            break;
    }
}





