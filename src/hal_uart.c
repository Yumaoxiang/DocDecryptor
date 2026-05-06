/*================================================================
  Copyright (c) 2021, Magic Wireless Solutions Co., Ltd. All rights reserved.
  Magic Wireless Solutions Proprietary and Confidential.
=================================================================*/

#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_uart.h"


/***************************************************************************
 * 名    称：uart2_init
 * 功    能：AUX_UART初始化
 * 返回值：1 成功  0失败
 * 说    明：
    1. RTE_Device.h中文件修改
    2. liot_uart.h中文件修改
 * 修	 改：25-07-06 速率正式修改为4800bps
            26-02-15 速率修改为9600bps，后续根据实际情况调整
 ***************************************************************************/
uint8_t uart2_init(){
    int ret                         = 0;
    liot_uart_config_s usart_config = {0};
    usart_config.baudrate           = LIOT_UART_BAUD_4800;
    usart_config.data_bit           = LIOT_UART_DATABIT_8;
    usart_config.flow_ctrl          = LIOT_FC_NONE;
    usart_config.stop_bit           = LIOT_UART_STOP_1;
    usart_config.parity_bit         = LIOT_UART_PARITY_NONE;
    usart_config.isPrintfPort       = TRUE;

    liot_rtos_task_sleep_s(1);
    liot_trace("Uart Printf Init: Baudrate %dbps \r\n", usart_config.baudrate);

    liot_pin_set_func(LIOT_UART2_RX_BIT, LIOT_UART2_TX_FUNC);
    liot_pin_set_func(LIOT_UART2_TX_BIT, LIOT_UART2_TX_FUNC);

    liot_uart_set_dcbconfig(LIOT_UART_PORT_2, &usart_config);

    ret = liot_uart_open(LIOT_UART_PORT_2);

    if (ret == LIOT_UART_SUCCESS){
        return 1;
    }
    else{
        return 0;
    }
}

/***************************************************************************
 * 优化说明：
 * - 结构化注释，初始化流程规范化，便于维护和扩展。
 * - 错误处理更清晰。
 * - 代码结构更清晰，便于维护和扩展。
 ***************************************************************************/

// 优化后的UART2初始化
uint8_t uart2_init_optimized(){
    int ret = 0;
    liot_uart_config_s usart_config = {0};
    usart_config.baudrate     = LIOT_UART_BAUD_4800;
    usart_config.data_bit     = LIOT_UART_DATABIT_8;
    usart_config.flow_ctrl    = LIOT_FC_NONE;
    usart_config.stop_bit     = LIOT_UART_STOP_1;
    usart_config.parity_bit   = LIOT_UART_PARITY_NONE;
    usart_config.isPrintfPort = TRUE;
    liot_rtos_task_sleep_s(1);
    liot_trace("Uart Printf Init: Baudrate %dbps \r\n", usart_config.baudrate);
    liot_pin_set_func(LIOT_UART2_RX_BIT, LIOT_UART2_TX_FUNC);
    liot_pin_set_func(LIOT_UART2_TX_BIT, LIOT_UART2_TX_FUNC);
    liot_uart_set_dcbconfig(LIOT_UART_PORT_2, &usart_config);
    ret = liot_uart_open(LIOT_UART_PORT_2);
    if (ret == LIOT_UART_SUCCESS){
        return 1;
    } else {
        liot_trace("UART2 open failed, ret=%d", ret);
        return 0;
    }
}
