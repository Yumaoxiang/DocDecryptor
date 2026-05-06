/***************************************************************************
 * @File Name: task_aep.c
 * @Author : ymx
 * @Version : 1.0
 * @Creat Date : 2024-05-09
 * @copyright Copyright (c) 2024 Lierda Science & Technology Group Co., Ltd.
 * 说    明：1. 电信AEP平台，MQTT协议，透传+json格式
 *   24-05-07 delivery_time、ping_timeout在实际使用中，如果发送等待返回时，是否上层等待冲突，需确认。
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "reset.h"

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_mqtt_client.h"
#include "liot_os.h"
#include "liot_type.h"
#include "sockets.h"
#include "liot_fs_api.h"

#include "hal_project.h"




#define ONENET_TASK_STACK_SIZE_8K   (8192 + 4)                         // 经验值4K，稳妥起见8K
static liot_task_t g_onenettaskRef = NULL;                             // 应该是task句柄

/******************************************************************************
*名    称：liot_create_onenet_task
*功    能：创建按键线程
*说    明：经验值4K，扩展到8K
*******************************************************************************/
void liot_create_onenet_task(void){
    LiotOSStatus_t result = liot_rtos_task_create(
                                &g_onenettaskRef,
                                ONENET_TASK_STACK_SIZE_8K,
                                APP_PRIORITY_NORMAL,
                                "OneNet_Task",
                                OneNet_Task,
                                NULL);
    if(result == 0){
        liot_trace("onenet task create success");
    }
    else{
        liot_trace("onenet task create fail");
    }          
}


/******************************************************************************
*名    称：OneNet_Task
*功    能：OneNet
*说    明：task AEP空间 8K
*修    改：
*******************************************************************************/
void OneNet_Task(void *pvParameters){
    liot_trace("---task onenet start----");
    cat1_init(en_CMD_POWERON);

    comm_msg_t recv;
    while(1){
        if (liot_rtos_queue_wait(platform_queue, (uint8_t*)&recv, sizeof(recv), LIOT_WAIT_FOREVER) == LIOT_OSI_SUCCESS) {
            log_received_message(&recv);
            // 根据来源区分处理
            switch (recv.source) {
                case TRIGGER_SOURCE_UART:
                    handle_uart_trigger(&recv);     // 处理UART触发的上报
                    break;
                case TRIGGER_SOURCE_KEY:
                    handle_key_trigger(&recv);      // 处理按键触发的上报
                    break;
                case TRIGGER_SOURCE_TIMER:
                    handle_timer_trigger(&recv);    // 处理定时器触发的上报，例如心跳或重连
                    break;
                case TRIGGER_SOURCE_PLATFORM:
                    handle_platform_trigger(&recv); // 处理平台下发的命令，例如执行对应操作或回复ACK
                    break;
                default:
                    break;
            }
        }
    }

    liot_trace("OneNet_Task exception occurred, delete OneNet_Task!");
    liot_rtos_task_delete(g_onenettaskRef);
}

