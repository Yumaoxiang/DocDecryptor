#include <stdio.h>
#include <string.h>
#include "lierda_app_main.h"
#include "liot_gpio.h"
#include "liot_os.h"
#include "mem_map.h"

#include "hal_project.h"


key_input_switch      key_switch_var;   // 按键输入变量，用于存储按键输入状态，实际并未使用
liot_queue_t key_switch_queue;          // 按键消息队列

/*******************************************************************************
* 名    称：gpio_int_key_common_cb
* 功    能：通用按键中断回调，用于处理按键触发事件
* 入口参数：argv - 按键信息结构体指针
* 优化说明：
*   - 合并所有按键中断回调为一个通用函数，减少重复代码，便于维护和扩展。
*******************************************************************************/
void gpio_int_key_common_cb(void *argv){
    comm_msg_t msg;
    msg.source = TRIGGER_SOURCE_KEY;
    msg.type = MSG_TYPE_KEY_TRIGGER_IO_STATUS_CHANGE;
    liot_rtos_queue_release(key_switch_queue, sizeof(msg), (uint8_t*)&msg, 0);
}

/*******************************************************************************
 * 名    称：key_switch_init
 * 功    能：初始化开关类按键GPIO
 * 说    明：
 * 修    改：
 *******************************************************************************/
void key_switch_init(){
    liot_pin_set_func(23, 4);                                                       // Modle Pin Num复选功能，Pin23为MAIN_RTS，Alt Func4对应GPIO20
    liot_gpio_init(SWITCH_KEY_1,LIOT_GPIO_INPUT,LIOT_FORCE_PULL_UP,LIOT_LVL_LOW);   // Pin23(MAIN_RTS)复选Alt Func4，Alt Func4对应GPIO20，初始化GPIO20
    liot_int_register(SWITCH_KEY_1,LIOT_EDGE_TRIGGER,LIOT_DEBOUNCE_DIS,LIOT_EDGE_FALLING,LIOT_FORCE_PULL_UP,gpio_int_key_common_cb,&key_switch_var.info1);
    liot_int_enable(SWITCH_KEY_1);

    #if KEY_SWITCH_1_ON_OFF_ENABLE
    liot_pin_set_func(21, 4);                                                       // Modle Pin Num复选功能，Pin21为MAIN_DCD，Alt Func4对应GPIO18
    liot_gpio_init(SWITCH_KEY_1_ON,LIOT_GPIO_INPUT,LIOT_FORCE_PULL_UP,LIOT_LVL_LOW);// Pin21(MAIN_DCD)复选Alt Func4，Alt Func4对应GPIO18，初始化GPIO18
    liot_int_register(SWITCH_KEY_1_ON,LIOT_EDGE_TRIGGER,LIOT_DEBOUNCE_DIS,LIOT_EDGE_FALLING,LIOT_FORCE_PULL_UP,gpio_int_key_common_cb,&key_switch_var.info1_on);
    liot_int_enable(SWITCH_KEY_1_ON);

    liot_pin_set_func(22, 4);                                                       // Modle Pin Num复选功能，Pin22为MAIN_CTS，Alt Func4对应GPIO19
    liot_gpio_init(SWITCH_KEY_1_OFF,LIOT_GPIO_INPUT,LIOT_FORCE_PULL_UP,LIOT_LVL_LOW);// Pin22(MAIN_CTS)复选Alt Func4，Alt Func4对应GPIO19，初始化GPIO19
    liot_int_register(SWITCH_KEY_1_OFF,LIOT_EDGE_TRIGGER,LIOT_DEBOUNCE_DIS,LIOT_EDGE_FALLING,LIOT_FORCE_PULL_UP,gpio_int_key_common_cb,&key_switch_var.info1_off);
    liot_int_enable(SWITCH_KEY_1_OFF);
    #endif

    #if KEY_SWITCH_2_3_4_ENABLE
    liot_pin_set_func(67, 4);
    liot_gpio_init(SWITCH_KEY_2,LIOT_GPIO_INPUT,LIOT_FORCE_PULL_UP,LIOT_LVL_LOW);              
    liot_int_register(SWITCH_KEY_2,LIOT_EDGE_TRIGGER,LIOT_DEBOUNCE_DIS,LIOT_EDGE_FALLING,LIOT_FORCE_PULL_UP,gpio_int_key_common_cb,&key_switch_var.info2);
    liot_int_enable(SWITCH_KEY_2);

    liot_pin_set_func(38, 0);                                                       // Modle Pin Num复选功能，Pin38为MAIN_TXD，Alt Func0对应GPIO6
    liot_gpio_init(SWITCH_KEY_3,LIOT_GPIO_INPUT,LIOT_FORCE_PULL_UP,LIOT_LVL_LOW);              
    liot_int_register(SWITCH_KEY_3,LIOT_EDGE_TRIGGER,LIOT_DEBOUNCE_DIS,LIOT_EDGE_FALLING,LIOT_FORCE_PULL_UP,gpio_int_key_common_cb,&key_switch_var.info3);
    liot_int_enable(SWITCH_KEY_3);

    liot_pin_set_func(39, 0);
    liot_gpio_init(SWITCH_KEY_4,LIOT_GPIO_INPUT,LIOT_FORCE_PULL_UP,LIOT_LVL_LOW);              
    liot_int_register(SWITCH_KEY_4,LIOT_EDGE_TRIGGER,LIOT_DEBOUNCE_DIS,LIOT_EDGE_FALLING,LIOT_FORCE_PULL_UP,gpio_int_key_common_cb,&key_switch_var.info4);
    liot_int_enable(SWITCH_KEY_4);
    #endif
}

/***************************************************************************
 * 名    称：key_switch_disable
 * 功    能：禁用开关类按键GPIO中断
 * 说    明：
 * 修    改：
 ***************************************************************************/
void key_switch_disable(){
    liot_int_disable(SWITCH_KEY_1);
    
    #if KEY_SWITCH_1_ON_OFF_ENABLE
    liot_int_disable(SWITCH_KEY_1_ON);
    liot_int_disable(SWITCH_KEY_1_OFF);
    #endif

    #if KEY_SWITCH_2_3_4_ENABLE
	liot_int_disable(SWITCH_KEY_2);
    liot_int_disable(SWITCH_KEY_3);
    liot_int_disable(SWITCH_KEY_4);
    #endif
}

/***************************************************************************
 * 名    称：key_switch_enable
 * 功    能：使能开关类按键GPIO中断
 * 说    明：
 * 修    改：
 ***************************************************************************/
void key_switch_enable(){
    liot_int_enable(SWITCH_KEY_1);

    #if KEY_SWITCH_1_ON_OFF_ENABLE
    liot_int_enable(SWITCH_KEY_1_ON);
    liot_int_enable(SWITCH_KEY_1_OFF);
    #endif

    #if KEY_SWITCH_2_3_4_ENABLE
	liot_int_enable(SWITCH_KEY_2);
    liot_int_enable(SWITCH_KEY_3);
    liot_int_enable(SWITCH_KEY_4);
    #endif
}

