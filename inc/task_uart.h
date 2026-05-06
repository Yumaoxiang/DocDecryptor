#ifndef __TASK_UART_H__
#define __TASK_UART_H__

#include "hal_project.h"

// UART 缓冲区大小定义
#define LIOT_UART_RX_BUFF_SIZE_64           64
#define LIOT_UART_TX_BUFF_SIZE_64           64
#define UART_PROTOCOL_DATA_OUT_SIZE_64      64

// 协议帧长度边界，便于维护与扩展
#define UART_PROTOCOL_MIN_FRAME_LEN     9        /* 最短合法帧长度（AA + CMD + msgID + CRC + FF 等） */
#define UART_PROTOCOL_MAX_FRAME_LEN     64       /* 超长帧阈值，超过则丢弃 */

// 平台下发开关命令后，2S 内收到的 MCU 开关命令，模组不在响应而是直接反馈，避免平台下发与本地 UART 冲突问题
#define TIME_PLATFORM_CMD_PROTECT_2S    2

extern liot_queue_t uart2_queue;



// UART 错误类型枚举
typedef enum {
    UART_FAULT_NONE = 0,            // 无错误
    UART_FAULT_RCV_OVERFLOW,        // 接收缓冲区溢出
    UART_FAULT_MCU_CMD_FREQ_HIGH,   // MCU 指令频率过高
    UART_FAULT_MCU_CMD_FREQ_LOW,    // MCU 指令频率过低
    UART_FAULT_PREAMBLE_FAIL,       // 协议起始位检查失败
    UART_FAULT_PROTOCOL_INVALID,    // 协议无效 (长度或结构不符)
    UART_FAULT_CRC_ERROR,           // CRC 校验失败
    UART_FAULT_ENDING_INVALID,      // 结束位检查失败
    UART_FAULT_UNKNOWN_COMMAND,     // 未知命令
    UART_FAULT_IO_STATUS_INVALID,   // IO 状态字符无效
    UART_FAULT_NO_FEEDBACK,         // 平台下发命令后未收到 MCU 反馈
} e_UART2_FAULT_TYPE;

// UART2 信息结构体
typedef struct uart2_info {
    unsigned int        receivedLength;         // 接收数据长度
    unsigned char       rcvBuff[LIOT_UART_RX_BUFF_SIZE_64]; // 接收缓冲区

    unsigned char       cmd3_sec_cnt;           // CMD3 状态帧发送计数器（按 2s 周期发送一次）
    unsigned int        cmd4_platform_rcv_cnt;  // CMD4 平台下发计数器（用于判断是否接收到过平台下发的 IO 开关命令，解决平台下发与本地 UART 冲突问题）
    unsigned int        cmd7_platform_rcv_cnt;  // CMD7 平台下发计数器（用于判断是否接收到过平台下发的电机控制命令，解决平台下发与本地 UART 冲突问题）
    unsigned int        cmd_vol_cur_update_cnt; // CMD5/CMD8 电压电流更新计数器（用于判断 3min 内更新了多少次，进行流量保护）
    unsigned int        cmd_vol_cur_timer_cnt;  // CMD5/CMD8 电压电流更新计时器（1s 自增，3min=180s 后清 0 计数器）
    unsigned int        heartbeatCounter;       // 心跳发送计数


    unsigned char       messageIDCounter;       // 模组发送消息 ID 计数器
    unsigned char       messageID[3];           // 记录模组发送的 msgID (用于模组主动发送时)


    unsigned int        cycle_18hour_count;     // 18 小时周期计数
    unsigned int        cmd_interaction_count_18hour; // 18 小时周期内命令交互次数

    unsigned char       isUARTSwitchIO;         // 是否接收到 UART IO 开关命令标志位：1-接收，0-未接收，用于平台上报时 msgID 赋值
    unsigned char       isRcvIoSwitch;          // 是否接收到平台或者本地 IO 开关命令标志位：1-接收，0-未接收，用于平台下发时判断是否需要推送 CMD4
    unsigned char       isIoSwitchFeedback;     // 是否收到 MCU 反馈标志位：1-收到，0-未收到，用于平台下发 IO 开关命令后的反馈检测
    unsigned char       isMemStateFeedback;     // 是否收到 MCU 反馈标志位：1-收到，0-未收到，用于平台下发状态记忆命令后的反馈检测
    e_UART2_FAULT_TYPE  faultType;              // 当前错误类型
    
    /* 用于反馈检查的发送状态记录 */
    uint8_t             lastSentIoState;        // 最近一次发送的 IO 状态（用于 CMD4 反馈检查）
    uint8_t             lastSentMemValue;       // 最近一次发送的记忆状态值（用于 CMD9 反馈检查：0x00/0x01/0x02）
    uint8_t             lastSentMotorMode;      // 最近一次发送的电机模式（用于 CMD7 反馈检查：0x00=HOLD, 0x01=JOG）
    uint8_t             lastSentMotorStatus;    // 最近一次发送的电机状态（用于 CMD7 反馈检查：0x01=STOP, 0x02=FORWARD, 0x03=REVERSE）
} uart2_info_t;
extern uart2_info_t uart2;



// UART 任务相关函数
void handle_uart_rx(liot_uart_port_number_e port, uint32 size);
void liot_create_uart_task(void);
void UART_Task(void *pvParameters);


void UART_Receive_Handler();


#endif
