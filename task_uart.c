/***************************************************************************************************************
 * @File Name: task_uart.c
 * @Author : ymx
 * @Version : 2.0
 * @Creat Date : 2026-02-15
 * @copyright Copyright (c) 2024 Lierda Science & Technology Group Co., Ltd.
 * 
 * 协    议：https://gqg3f78cgn.feishu.cn/docx/BOP0dgNzeo6wlQxVecDc69Own3b?from=from_copylink
 * 说    明：
 ***************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_mqtt_client.h"
#include "liot_os.h"
#include "liot_type.h"
#include "sockets.h"
#include "liot_fs_api.h"

#include "lierda_app_main.h"
#include "liot_gpio.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_uart.h"
#include "task_uart.h"
#include "hal_project.h"
#include "drv_hex_protocol.h"
#include "drv_uart_protocol.h"


#ifndef MIN
#define MIN(a, b)              ((a) < (b) ? (a) : (b))
#endif


#define UART_TASK_STACK_SIZE_4K     (4096 + 4)                          // 经验值4K
// static liot_StaticTask_t uart_task_mem;                                 // 功能暂时未知
// __ALIGNED(8) static uint8_t uart_task_stack[UART_TASK_STACK_SIZE_4K];   // 固定格式

// 全局变量定义
uart2_info_t uart2;

// 全局变量
liot_queue_t uart2_queue;

static liot_task_t g_uarttaskRef = NULL;                                // UART任务句柄



unsigned char uart_send_heart_and_io[15] = "AA20000Y00XXFF";            // 心跳包和IO口状态
unsigned char uart_send_vol_cur_fb[30] = "AAFV222.0A037.539XXFF";
unsigned char uart_send_param_get[22]  =  "AAG00000000000037XXFF";      // 参数获取命令
unsigned char uart_send_param_limit[28] ="AA6V480.0V100.0A080.037XXFF"; // 参数获取命令
unsigned char uart_send_alarm_fd[17] ="AAWOV265.000XXFF";               // 报警反馈命令


unsigned char uart_send_motor[12] = "AA7LT73XXFF";                      // 电机正反转命令
unsigned char uart_send_motor_fb[12] = "AAFJF72XXFF";                   // 电机反馈命令

unsigned char uart_send_motor_ext[25] = "AA8LTV220.0A030.0000XXFF";        // 电机扩展正反转命令
unsigned char uart_send_motor_ext_fb[25] = "AAFLTV220.0A030.0000XXFF";     // 电扩展机反馈命令



/***************************************************************************
 * 名    称：liot_uart_notify_cb_app
 * 功    能：AUX_UART回调，处理接收和发送事件，增强异常处理和边界检查
 * 参数说明：
 *   ind_type - 中断类型
 *   port     - UART端口号
 *   size     - 接收数据长度
 ***************************************************************************/
void liot_uart_notify_cb_app(uint32 ind_type, liot_uart_port_number_e port, uint32 size){
    switch (ind_type){
        case LIOT_UART_RX_OVERFLOW_IND:
        case LIOT_UART_RX_RECV_DATA_IND:
            handle_uart_rx(port, size);
            break;
        case LIOT_UART_TX_FIFO_COMPLETE_IND:
            // 可扩展TX完成事件处理
            break;
    }
}

/***************************************************************************
 * 名    称：handle_uart_rx
 * 功    能：处理UART接收事件，增强异常处理和边界检查
 * 参数说明：
 *   port - UART端口号
 *   size - 接收数据长度
 * 说    明：
 *   1. 增加接收数据长度检查，防止缓冲区溢出。
 *   2. 增加接收数据完整性检查，确保数据正确接收。
 *   3. 增加错误处理机制，记录接收错误类型。
 ***************************************************************************/
void handle_uart_rx(liot_uart_port_number_e port, uint32 size)
{
    int read_len = 0;
    unsigned int real_size = 0;

    memset(uart2.rcvBuff, 0, LIOT_UART_RX_BUFF_SIZE_64);
    uart2.receivedLength = size;
    if (uart2.receivedLength >= LIOT_UART_RX_BUFF_SIZE_64) {
        uart2.faultType = UART_FAULT_RCV_OVERFLOW;
        uart2.receivedLength = LIOT_UART_RX_BUFF_SIZE_64;
    }
    read_len = liot_uart_read(port, (unsigned char *)uart2.rcvBuff, uart2.receivedLength);
    if (size > (uint32)read_len)
        size -= read_len;
    else
        size = 0;
    while (size > 0) {
        real_size = MIN(size, LIOT_UART_RX_BUFF_SIZE_64);
        read_len = liot_uart_read(port, (unsigned char *)uart2.rcvBuff, real_size);
        if ((read_len > 0) && (size >= (uint32)read_len)) {
            size -= read_len;
        } else {
            break;
        }
    }

    comm_msg_t msg;
    msg.source = TRIGGER_SOURCE_UART;
    msg.type = MSG_TYPE_UART_RCV_TRIGGER;
    liot_rtos_queue_release(uart2_queue, sizeof(msg), (uint8_t*)&msg, 0);
}



/* ----------------- UART 协议解析相关函数 ----------------- */



/******************************************************************************
*名    称：liot_create_uart_task
*功    能：创建按键线程
*说    明：经验值4K，扩展到8K
*******************************************************************************/
void liot_create_uart_task(void){
    LiotOSStatus_t result = liot_rtos_task_create(
                                &g_uarttaskRef,
                                UART_TASK_STACK_SIZE_4K,
                                APP_PRIORITY_NORMAL,
                                "UART_Task",
                                UART_Task,
                                NULL);
    if(result == 0){
        liot_trace("UART2 task create success");
    }
    else{
        liot_trace("UART2 task create fail");
    }          
}

/******************************************************************************
*名    称：UART_Task
*功    能：串口线程
*说    明：task UART2空间 8K
*修    改：
*******************************************************************************/
void UART_Task(void *pvParameters){
    liot_trace("---task uart start----");

    if(uart2_init()){
        liot_uart_register_cb(LIOT_UART_PORT_2, liot_uart_notify_cb_app);
        liot_uart_tx_way_config(LIOT_UART_TX_OPAQ);
        liot_trace("uart2 init SUCCESS");
    } else {
        liot_trace("uart2 init FAIL and Delete!!!");
        liot_rtos_task_delete(NULL);
        return;                       
    }
    liot_rtos_queue_create(&uart2_queue, sizeof(comm_msg_t), 5);    // 创建队列：item_size = sizeof(comm_msg_t)，depth = 5

    #if UART_PROTOCOL_FORMAT_ASCII
    drv_send_cmd3_push("PWR");                                      // 发送状态帧: AA3SWTV3.0PWR01XXFF
    #endif

    #if MEM_STATE_CAT1_FLASH
    // liot_rtos_task_sleep_ms(50);                                    // 防止连续打印粘包，或MCU无法识别
    // drv_send_cmd9_push("MEM");                                      // 发送状态记忆参数帧，这里不在等待是否MCU返回   
    #endif

    while (1){
        comm_msg_t msg;
        if (liot_rtos_queue_wait(uart2_queue, (uint8_t*)&msg, sizeof(msg), LIOT_WAIT_FOREVER) == LIOT_OSI_SUCCESS) {
            switch (msg.source) {
            case TRIGGER_SOURCE_UART:
                UART_Receive_Handler();                             // 模组接收到UART串口发来数据，处理接收到的数据，解析协议并执行相应命令
                break;
            default:
                break;
            }
        }
    }
}


/******************************************************************************
*名    称：UART_Receive_Handler
*功    能：UART接收到数据，解析、处理
*说    明：
*修    改：
*******************************************************************************/
void UART_Receive_Handler(){
    /* 可选：检查命令频率 */
    // uart_check_mcu_cmd_frequency();

#if (UART_PROTOCOL_FORMAT_HEX)
    /* Hex 协议处理 */
    liot_trace("UART2 receive data, len=%d, data:", uart2.receivedLength);
    hex_protocol_print_buffer((uint8_t*)uart2.rcvBuff, uart2.receivedLength);
    hex_protocol_receive_handler((uint8_t*)uart2.rcvBuff, uart2.receivedLength);

#elif (UART_PROTOCOL_FORMAT_ASCII)
    liot_trace("UART2 receive data, len=%d, data=%s", uart2.receivedLength, uart2.rcvBuff);
    /* ASCII 协议处理 */
    /* 1) 检查帧头与最短长度，包含最短帧长度保护与帧头 ("AA") 校验*/
    if (!uart_protocol_check_preamble(uart2.rcvBuff, uart2.receivedLength)) {
        liot_trace("UART proto check 'AA' Fail");
        return;
    }

    /* 2) 检查协议完整性（最大长度/CRC 校验（支持 "XX" 豁免）/尾帧 ("FF") 校验） */
    if (!uart_protocol_check_protocol(uart2.rcvBuff, uart2.receivedLength)) {
        liot_trace("UART proto check Fail");
        return;
    }

    /* 3) 执行命令解析与本地处理（仅在模组/MCU 本地处理，不向平台透传） */
    uart_receive_execute_command();
#endif
}

