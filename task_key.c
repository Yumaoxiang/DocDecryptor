#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_gpio.h"

#include "hal_project.h"
#include "hal_gpio.h"
#include "task_key.h"
#include "drv_cloud_common.h"

#define KEY_TASK_STACK_SIZE_4K     (4096 + 4) // 4 字节为了对齐

static liot_task_t g_keytaskRef = NULL;


/******************************************************************************
*名    称：liot_create_key_task
*功    能：创建按键线程
*说    明：经验值 4K
*******************************************************************************/
void liot_create_key_task(void){
    // 创建按键队列
    liot_rtos_queue_create(&key_switch_queue, sizeof(comm_msg_t), 5);

    LiotOSStatus_t result = liot_rtos_task_create(
                                &g_keytaskRef,
                                KEY_TASK_STACK_SIZE_4K,
                                APP_PRIORITY_NORMAL,
                                "Key_Task",
                                Key_Task,
                                NULL);
    if(result == 0){
        liot_trace("Key task create success");
    }
    else{
        liot_trace("Key task create fail");
    }          
}

/******************************************************************************
*名    称：Key_Task
*功    能：按键线程
*说    明：task KEY 空间 4K
*修    改：
*******************************************************************************/
void Key_Task(void *pvParameters){
    liot_trace("---task key start----");
    key_switch_init();                          // 开关类按键 GPIO 初始化

    while (1){
        comm_msg_t msg;
        if (liot_rtos_queue_wait(key_switch_queue, (uint8_t*)&msg, sizeof(msg), LIOT_WAIT_FOREVER) == LIOT_OSI_SUCCESS) {
            liot_trace("Key event received, source=%s, type=%s", trigger_source_to_str(msg.source), trigger_type_to_str(msg.type));
            switch (msg.source) {
            case TRIGGER_SOURCE_KEY:
                liot_trace("Key event triggered, type=%s", trigger_type_to_str(msg.type));
                key_switch_disable();
                key_switch_press_check();
                key_switch_enable();
                break;
            default:
                break;
            }
        }
    }

    liot_trace("Key_Task exception occurred, delete Key_Task!");
    liot_rtos_task_delete(g_keytaskRef);
}

/******************************************************************************
* 名    称：key_switch_press_check
* 功    能：统一检测所有按键的按下事件
* 说    明：依次调用各按键检测函数，根据宏定义决定是否启用对应按键检测
* 注    意：KEY_SWITCH_1_ON_OFF_ENABLE、KEY_SWITCH_2_3_4_ENABLE 需在配置中开启
******************************************************************************/
void key_switch_press_check(void){
    key_switch_press_check_key1();

    #if KEY_SWITCH_1_ON_OFF_ENABLE
    key_switch_press_check_key1_on_off();
    #endif

    #if KEY_SWITCH_2_3_4_ENABLE
    key_switch_press_check_key2();
    key_switch_press_check_key3();
    key_switch_press_check_key4();
    #endif
}

/*******************************************************************************
 * 名  称：key_switch_press_check_key1
 * 功  能: 检测开关按键是否按下
 * 说  明: 
 *返回值 ：0 无按键按下  1 有按键按下
 *******************************************************************************/
uint8_t key_switch_press_check_key1(){
    liot_gpio_lvl_mode_e key_level;                     //读取到按键的状态，按键松手检测

    if(key_switch_debounce_common(SWITCH_KEY_1, KEY_DEBOUNCE_160MS)){        // 检查 KEY1 是否按下
        liot_trace("Switch Key1 is pressed and pass debounce");

        // 切换 IO 状态
        g_io.out[3].now_status = (g_io.out[3].now_status == enIO_OUT_ON) ? enIO_OUT_OFF : enIO_OUT_ON;
        liot_trace("GPIO output switched to: %s", (g_io.out[3].now_status == enIO_OUT_ON) ? "IO_OUT_1_H" : "IO_OUT_1_L");

        // 确认 UART 反馈，根据反馈结果执行操作（如果没有使能 UART 通信，直接返回成功，即无需 UART 反馈确认）
        e_SWITCH_KEY_CONFIRM_RESULT confirmResult = KeySwich_ConfirmUART_Feedback();

        // 根据确认结果执行操作
        strcpy(cat1.msgIDStr, "K1_ALL");
        KeySwich_Excute(confirmResult);

        // 等待按键松手
        do{                                             // 等待松手
            liot_rtos_task_sleep_ms(20);                // 检测周期 20mS
            liot_gpio_get_level(SWITCH_KEY_1,&key_level);
        }while(key_level != LIOT_LVL_HIGH);
        liot_trace("Switch Key1 is released");          //输出打印信息

        return 1;
    }
    
    return 0;
}

/*******************************************************************************
 * 名  称：key_switch_press_check_key1_on_off
 * 功  能：检测开关按键 KEY1_ON 和 KEY1_OFF 是否按下
 * 说  明：分别检测 KEY1_ON 和 KEY1_OFF 按键，根据按下的按键设置对应的 IO 状态
 * 返回值：0 无按键按下  1 有按键按下
 *******************************************************************************/
uint8_t key_switch_press_check_key1_on_off(){
    liot_gpio_lvl_mode_e key_level;

    // 检测 KEY1_ON 按键
    if(key_switch_debounce_common(SWITCH_KEY_1_ON, KEY_DEBOUNCE_30MS)){
        liot_trace("Switch Key1_ON is pressed and pass debounce");

        // 设置 IO 状态为 ON
        g_io.out[3].now_status = enIO_OUT_ON;
        liot_trace("GPIO output switched to: IO_OUT_1_H");

        // 确认 UART 反馈
        e_SWITCH_KEY_CONFIRM_RESULT confirmResult = KeySwich_ConfirmUART_Feedback();

        // 根据确认结果执行操作
        strcpy(cat1.msgIDStr, "K1_O_N");
        KeySwich_Excute(confirmResult);

        // 等待按键松手
        do{
            liot_rtos_task_sleep_ms(20);
            liot_gpio_get_level(SWITCH_KEY_1_ON, &key_level);
        }while(key_level != LIOT_LVL_HIGH);
        liot_trace("Switch Key1_ON is released");

        return 1;
    }

    // 检测 KEY1_OFF 按键
    if(key_switch_debounce_common(SWITCH_KEY_1_OFF, KEY_DEBOUNCE_30MS)){
        liot_trace("Switch Key1_OFF is pressed and pass debounce");

        // 设置 IO 状态为 OFF
        g_io.out[3].now_status = enIO_OUT_OFF;
        liot_trace("GPIO output switched to: IO_OUT_1_L");

        // 确认 UART 反馈
        e_SWITCH_KEY_CONFIRM_RESULT confirmResult = KeySwich_ConfirmUART_Feedback();

        // 根据确认结果执行操作
        strcpy(cat1.msgIDStr, "K1_OFF");
        KeySwich_Excute(confirmResult);

        // 等待按键松手
        do{
            liot_rtos_task_sleep_ms(20);
            liot_gpio_get_level(SWITCH_KEY_1_OFF, &key_level);
        }while(key_level != LIOT_LVL_HIGH);
        liot_trace("Switch Key1_OFF is released");

        return 1;
    }
    
    return 0;
}

/*******************************************************************************
 * 名  称：key_switch_press_check_key2
 * 功  能：检测开关按键 KEY2 是否按下
 * 说  明：检测 KEY2 按键，切换对应的 IO 状态
 * 返回值：0 无按键按下  1 有按键按下
 *******************************************************************************/
uint8_t key_switch_press_check_key2(){
    liot_gpio_lvl_mode_e key_level;

    if(key_switch_debounce_common(SWITCH_KEY_2, KEY_DEBOUNCE_160MS)){
        liot_trace("Switch Key2 is pressed and pass debounce");

        // 切换 IO 状态
        g_io.out[2].now_status = (g_io.out[2].now_status == enIO_OUT_ON) ? enIO_OUT_OFF : enIO_OUT_ON;
        liot_trace("GPIO output switched to: %s", (g_io.out[2].now_status == enIO_OUT_ON) ? "IO_OUT_2_H" : "IO_OUT_2_L");

        // 确认 UART 反馈
        e_SWITCH_KEY_CONFIRM_RESULT confirmResult = KeySwich_ConfirmUART_Feedback();

        // 根据确认结果执行操作
        strcpy(cat1.msgIDStr, "K2_ALL");
        KeySwich_Excute(confirmResult);

        // 等待按键松手
        do{
            liot_rtos_task_sleep_ms(20);
            liot_gpio_get_level(SWITCH_KEY_2, &key_level);
        }while(key_level != LIOT_LVL_HIGH);
        liot_trace("Switch Key2 is released");

        return 1;
    }
    
    return 0;
}

/*******************************************************************************
 * 名  称：key_switch_press_check_key3
 * 功  能：检测开关按键 KEY3 是否按下
 * 说  明：检测 KEY3 按键，切换对应的 IO 状态
 * 返回值：0 无按键按下  1 有按键按下
 *******************************************************************************/
uint8_t key_switch_press_check_key3(){
    liot_gpio_lvl_mode_e key_level;

    if(key_switch_debounce_common(SWITCH_KEY_3, KEY_DEBOUNCE_160MS)){
        liot_trace("Switch Key3 is pressed and pass debounce");

        // 切换 IO 状态
        g_io.out[1].now_status = (g_io.out[1].now_status == enIO_OUT_ON) ? enIO_OUT_OFF : enIO_OUT_ON;
        liot_trace("GPIO output switched to: %s", (g_io.out[1].now_status == enIO_OUT_ON) ? "IO_OUT_3_H" : "IO_OUT_3_L");

        // 确认 UART 反馈
        e_SWITCH_KEY_CONFIRM_RESULT confirmResult = KeySwich_ConfirmUART_Feedback();

        // 根据确认结果执行操作
        strcpy(cat1.msgIDStr, "K3_ALL");
        KeySwich_Excute(confirmResult);

        // 等待按键松手
        do{
            liot_rtos_task_sleep_ms(20);
            liot_gpio_get_level(SWITCH_KEY_3, &key_level);
        }while(key_level != LIOT_LVL_HIGH);
        liot_trace("Switch Key3 is released");

        return 1;
    }
    
    return 0;
}

/*******************************************************************************
 * 名  称：key_switch_press_check_key4
 * 功  能：检测开关按键 KEY4 是否按下
 * 说  明：检测 KEY4 按键，切换对应的 IO 状态
 * 返回值：0 无按键按下  1 有按键按下
 *******************************************************************************/
uint8_t key_switch_press_check_key4(){
    liot_gpio_lvl_mode_e key_level;

    if(key_switch_debounce_common(SWITCH_KEY_4, KEY_DEBOUNCE_160MS)){
        liot_trace("Switch Key4 is pressed and pass debounce");

        // 切换 IO 状态
        g_io.out[0].now_status = (g_io.out[0].now_status == enIO_OUT_ON) ? enIO_OUT_OFF : enIO_OUT_ON;
        liot_trace("GPIO output switched to: %s", (g_io.out[0].now_status == enIO_OUT_ON) ? "IO_OUT_4_H" : "IO_OUT_4_L");

        // 确认 UART 反馈
        e_SWITCH_KEY_CONFIRM_RESULT confirmResult = KeySwich_ConfirmUART_Feedback();

        // 根据确认结果执行操作
        strcpy(cat1.msgIDStr, "K4_ALL");
        KeySwich_Excute(confirmResult);

        // 等待按键松手
        do{
            liot_rtos_task_sleep_ms(20);
            liot_gpio_get_level(SWITCH_KEY_4, &key_level);
        }while(key_level != LIOT_LVL_HIGH);
        liot_trace("Switch Key4 is released");

        return 1;
    }
    
    return 0;
}


/*******************************************************************************
 * 名    称：key_switch_debounce_common
 * 功    能: 开关类按键消抖检测
 * 入口参数:  liot_gpio_port_t gpio 按键GPIO端口
 *            uint8_t debounce_count 消抖时间，单位：10mS
 * 返回值 : 0 无按键按下  1 有按键按下
 * 示    例：key_switch_debounce_common(SWITCH_KEY_1, KEY_DEBOUNCE_160MS);
 * 说   明：
 *******************************************************************************/
uint8_t key_switch_debounce_common(liot_gpio_num_e gpio, uint8_t debounce_count){
    liot_gpio_lvl_mode_e key_level;
    for(uint8_t cnt = 0; cnt < debounce_count; cnt++){
        liot_gpio_get_level(gpio, &key_level);
        if(LIOT_LVL_HIGH == key_level){
            return 0;
        }
        liot_rtos_task_sleep_ms(10);
    }
    return 1;
}

/*******************************************************************************
*名    称：KeySwich_ConfirmUART_Feedback
*功    能：按键按下，确认UART是否有返回
*入口参数：无
*出口参数：e_SWITCH_KEY_CONFIRM_RESULT 按键确认结果
* 返回值说明：
*   - en_SWITCH_KEY_CONFIRM_SUC: UART 反馈成功
*   - en_SWITCH_KEY_CONFIRM_FAIL_UART_FEEDBACK: 未收到 UART 反馈
*******************************************************************************/
e_SWITCH_KEY_CONFIRM_RESULT KeySwich_ConfirmUART_Feedback() {
#if UART_COMM_ENABLE
    uart2.isIoSwitchFeedback = 0;                       // 收到平台下发的IO切换命令，等待UART反馈ACK

    // 直接发送 UART 消息
    for (int i = 0; i < 3; i++) {
        liot_trace("[UART2] drv_send_cmd4_push_%s %d times", 
                   UART_PROTOCOL_FORMAT_ASCII ? "str" : "hex", i+1);
        
        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd4_push_str();                           // ASCII 协议：发送 CMD4 推送帧
        #elif UART_PROTOCOL_FORMAT_HEX
        drv_send_cmd4_push_hex();                           // Hex 协议：发送 CMD4 推送帧
        #endif
        
        // 等待UART反馈
        for (int j = 0; j < 50; j++) {
            liot_rtos_task_sleep_ms(10);                // 每次等待10ms
            if (uart2.isIoSwitchFeedback) {
                liot_trace("-------> UART2 IO switch feedback received <-------");
                return en_SWITCH_KEY_CONFIRM_SUC;
            }
        }
        liot_trace("UART2 IO switch feedback wait timeout, retrying...");
    }
    
    // 如果多次发送仍未收到反馈，返回失败
    uart2.faultType = UART_FAULT_NO_FEEDBACK;
    liot_trace("UART2 IO switch feedback not received after retries");
    return en_SWITCH_KEY_CONFIRM_FAIL_UART_FEEDBACK;  // 未收到UART反馈，返回执行失败
#endif

    // 如果没有开启UART，则直接返回 SUC
    return en_SWITCH_KEY_CONFIRM_SUC;                   // 
}


/*******************************************************************************
*名    称：KeySwich_Excute
*功    能：根据确认结果，执行IO切换操作并反馈平台
*入口参数：confirm_result 按键确认结果
*说    明：
*修    改：
*******************************************************************************/
void KeySwich_Excute(e_SWITCH_KEY_CONFIRM_RESULT confirm_result) {
    // Handle IO state updates based on confirmation result
    if (confirm_result == en_SWITCH_KEY_CONFIRM_SUC) {
        handle_successful_confirmation();   // 更新 g_io.out 数组，更新 IO 状态并写入 Flash

        // 发送平台消息，通知 IO 状态改变
        comm_msg_t msg;
        msg.source = TRIGGER_SOURCE_KEY;
        msg.type   = MSG_TYPE_KEY_TRIGGER_IO_STATUS_CHANGE;
        LiotOSStatus_t ret = liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0);
        if (ret != LIOT_OSI_SUCCESS) {
            liot_trace("platform_queue full, Key msg dropped, source=%s, type=%s", trigger_source_to_str(msg.source), trigger_type_to_str(msg.type));
        } else {
            liot_trace("Key IO status changed, message sent to platform_queue, source=%s, type=%s", trigger_source_to_str(msg.source), trigger_type_to_str(msg.type));
        }
        return;
    } 
    else {
        handle_failed_confirmation();       // g_io.out 数组状态回滚到之前状态
        // Send feedback based on confirmation result
        switch (confirm_result) {
            case en_SWITCH_KEY_CONFIRM_FAIL_UART_FEEDBACK:
                iot_datasend_fault(en_FAULT_UART_NO_FEEDBACK);  // Send UART feedback fault
                break;

            default:
                break;
        }
    }
}
