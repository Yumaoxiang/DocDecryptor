#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "lierda_app_main.h"
#include "liot_os.h"


#include "hal_project.h"
#include "task_fota.h"


#define FOTA_TASK_STACK_SIZE_4K   (4096 + 4)                            // 经验值4K，后续增加按键扩展8K
// static liot_StaticTask_t fota_task_mem_A;                                  // 功能暂时未知
// __ALIGNED(8) static uint8_t fota_task_stack_A[FOTA_TASK_STACK_SIZE_4K];    // 固定格式
static liot_task_t g_fota_taskRef_A = NULL;                                 // 应该是task句柄

/******************************************************************************
*名    称：liot_create_key_task
*功    能：创建按键线程
*说    明：经验值4K，后续如果增加可能需要按键扩展到8K
*******************************************************************************/
void liot_create_fota_task(void){
    LiotOSStatus_t result = liot_rtos_task_create(
                                &g_fota_taskRef_A,
                                FOTA_TASK_STACK_SIZE_4K,
                                APP_PRIORITY_NORMAL,
                                "Fota_Task_AEP",
                                Fota_Task_AEP,
                                // fota_task_stack_A,
                                // &fota_task_mem_A,
                                NULL);
    if(result == 0){
        liot_trace("fota task create success");
    }
    else{
        liot_trace("fota task create fail, result = %d", result);
    }          
}


/******************************************************************************
*名    称：Fota_Task
*功    能：Fota线程
*说    明：Fota_Task空间 1K*4=4K
*修    改：
*******************************************************************************/
void Fota_Task_AEP(void *pvParameters){
    liot_trace("---task fota start----");

    liot_fota_http_nvm_thread2();

    liot_trace("Fota exception occurred, delete Key_Task()!");
    liot_osi_errcode_e ret = liot_rtos_task_delete(g_fota_taskRef_A);
    if (ret != LIOT_OSI_SUCCESS){
        liot_trace("fota task deleted failed");

    }
}


