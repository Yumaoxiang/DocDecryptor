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
 

#define AEP_TASK_STACK_SIZE_8K   (8192 + 4)                         // 经验值4K，稳妥起见8K
// static liot_StaticTask_t aep_task_mem;                              // 功能暂时未知
// __ALIGNED(8) static uint8_t aep_task_stack[AEP_TASK_STACK_SIZE_8K]; // 固定格式
static liot_task_t g_aeptaskRef = NULL;                             // 应该是task句柄


/******************************************************************************
*名    称：liot_create_aep_task
*功    能：创建按键线程
*说    明：经验值4K，扩展到8K
*******************************************************************************/
void liot_create_aep_task(void){
    LiotOSStatus_t result = liot_rtos_task_create(
                                &g_aeptaskRef,
                                AEP_TASK_STACK_SIZE_8K,
                                APP_PRIORITY_NORMAL,
                                "AEP_Task",
                                AEP_Task,
                                // aep_task_stack,
                                // &aep_task_mem,
                                NULL);
    if(result == 0){
        liot_trace("aep task create success");
    }
    else{
        liot_trace("aep task create fail");
    }          
}


/******************************************************************************
*名    称：AEP_Task
*功    能：按键线程
*说    明：task AEP空间 8K
*修    改：
*******************************************************************************/
void AEP_Task(void *pvParameters){
    liot_trace("---task aep start----");
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

    liot_trace("AEP_Task exception occurred, delete AEP_Task!");
    liot_rtos_task_delete(g_aeptaskRef);
}


/*******************************************************************************
*名    称：UartVoltageUpdateCheck
*功    能：检查UART电压更新
*入口参数：无
*说    明：UART电压更新检查函数，主要用于定时检查UART电压状态
*修    改：26-06-29 因为UART更新过于频繁，这部分主动上报暂时不执行，改为平均值计算，在心跳包中进行周期性赋平均值
*******************************************************************************/
void UartVoltageUpdateCheck(){
    // if (!cat1.initSucFlag) return;          // 如果初始化失败，则不做更新，初始化失败重连在心跳包中执行，不在对应的命令中执行
    // if (!cat1.param.UpdateFlag) return;     // 如果没有更新标志，则不进行检查
    // cat1.param.UpdateFlag = 0;              // 清除更新标志
    // liot_trace("UART: Voltage and Current Value update check");

    // if(datasend_n_times_with_reconnect(en_CMD_UART_SWITCH_PARAM_UPDATE, 2)){
    //     liot_trace("UART: Voltage and Current Value update success");
    //     cat1.heart_cnt = 0;                 // 数据发送成功，心跳包对应延迟
    // }
    
    // if (!cat1.param.UpdateFlag) return;     // 如果没有更新标志，则不进行检查
    // cat1.param.UpdateFlag = 0;              // 清除更新标志

}

/*******************************************************************************
*名    称：UartAlarmUpdateCheck
*功    能：检查UART报警更新，如果UART有报警则上传到平台
*入口参数：无
*说    明：UART报警更新检查函数，主要用于定时检查UART报警状态
          1. 同一个报警类型，在1小时内仅上报一次；
          2. 若1小时后再次发生同样的报警，允许重新上报；
*修    改：
*******************************************************************************/
void UartAlarmUpdateCheck(){
    if (!cat1.initSucFlag) return;      // 如果初始化失败，则不做更新，初始化失败重连在心跳包中执行，不在对应的命令中执行
    if (!cat1.alarm_flag) return;       // 如果没有更新标志，则不进行检查
    cat1.alarm_flag = 0;                // 清除更新标志
    liot_trace("aep_data_alarm: %s", aep_data_alarm);

    if(datasend_n_times_with_reconnect(en_CMD_DEV_ALARM, 2)){
        liot_trace("Alarm update AEP success");
        cat1.heart_cnt = 0;             // 数据发送成功，心跳包对应延迟
    }
    else{
        liot_trace("UART: Alarm update AEP fail");
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// 检查字段是否合法（是"FFFFF"或全为数字/带1个小数点）
int is_valid_param(const char *str) {
    if (strncmp(str, "FFFFF", 5) == 0)
        return 1;

    int dot_count = 0;

    for (int i = 0; i < 5; i++) {
        if (str[i] == '.') {
            dot_count++;
            if (dot_count > 1) {
                liot_trace("Error: Too many decimal points in limit data: %s", str);
                return 0;
            }
        } else if (!isdigit((unsigned char)str[i])) {
            liot_trace("Error: Invalid character in limit data: %c", str[i]);
            return 0;
        }
    }

    return 1;
}

// 解析函数
int SetLimit_parse_param_string(const char *input) {
    char *ov = strstr(input, "OV:");
    char *uv = strstr(input, "UV:");
    char *oc = strstr(input, "OC:");

    if (!ov || !uv || !oc) {
        liot_trace("Error: Missing 'OV' 'UV' 'OC' in limit data");
        return 0; // 缺少字段
    }

    char temp[6] = {0};

    // 提取 OV 值（取 : 后的5位）
    strncpy(temp, ov + 3, 5);
    if (!is_valid_param(temp)) return 0;
    strncpy(cat1.param.overVoltage, temp, 5);
    cat1.param.overVoltage[5] = '\0';

    // 提取 UV 值
    strncpy(temp, uv + 3, 5);
    if (!is_valid_param(temp)) return 0;
    strncpy(cat1.param.underVoltage, temp, 5);
    cat1.param.underVoltage[5] = '\0';

    // 提取 OC 值
    strncpy(temp, oc + 3, 5);
    if (!is_valid_param(temp)) return 0;
    strncpy(cat1.param.overCurrent, temp, 5);
    cat1.param.overCurrent[5] = '\0';

    return 1; // 解析成功
}


/*******************************************************************************
*名    称：SeverDataRead_ModlInfo()
*功    能：服务器下发"获取开机信息"命令
*说    明：
*修    改：
*******************************************************************************/
void SeverDataRead_ModlInfo(){
    SeverDataRead_CmdExcute_Extract_msgID();

    cat1.rcv.poweron_info_flag_get = 1;                                     // 设置标志位，表示需要获取开机数据
    datasend_n_times_with_reconnect(en_CMD_POWERON, 2);                     // 发送开机数据
}


/*******************************************************************************
*名    称：SeverDataRead_SetLimit()
*功    能：服务器下发"设置限流"命令
*说    明：
*修    改：
*******************************************************************************/
void SeverDataRead_SetLimit(){
    
}


/*******************************************************************************
*名    称：SeverDataRead_ParamGet()
*功    能：服务器下发"同步获取信息"命令
*说    明：
*修    改：25-07-10 根据OND协议不在发送UART，而是直接从cat1.vol中提取
*******************************************************************************/
void SeverDataRead_ParamGet(){
    memset(cat1.rcv.SerRandomID, 0, sizeof(cat1.rcv.SerRandomID));
    extractJson(cat1.rcv.buf, "msgID", cat1.rcv.SerRandomID, sizeof(cat1.rcv.SerRandomID)); // 反馈 ACK 时使用
    liot_trace("    3. cat1.rcv.SerRandomID: %s", cat1.rcv.SerRandomID);

    if(datasend_n_times_with_reconnect(en_CMD_VOL_AND_CUR_ACK, 2)){
        liot_trace("UART: Voltage and Current Value update ACK success");
        cat1.heart_cnt = 0;                 // 数据发送成功，心跳包对应延迟
        uart2.cmd_vol_cur_update_cnt = 0;   // 用户体验升级，点击小程序后，重新主动更新电压电流数据 n 次
    }
}


