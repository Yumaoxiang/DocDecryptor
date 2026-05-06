/***************************************************************************
 * 名    称：hal_gpio.c
 * 功    能：GPIO初始化
 * 版    本：V1.0
 * 创建时间: 2025-11-26
 * 说    明：1. GPIO输出需要先设置复选功能，然后根据复选功能进行初始化
 *       
 *       NT26KCNB00NNC     
 *    ------------------
 *   |                  | 
 *   |             Pin97|-->LED_NET
 *   |                  |
 * 	 |             Pin16|-->
 *   |             Pin20|-->
 *   |             Pin25|-->
 *   |                  |
 * 
 * 
 ***************************************************************************/
#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio.h"
#include "liot_os.h"
#include "mem_map.h"

#include "hal_gpio.h"
#include "hal_project.h"

/***************************************************************************
 * 名    称：gpio_out_init
 * 功    能：GPIO输出初始化（开关产品或电机产品）
 * 说    明：
// liot_wakeup_isr_deinit(LIOT_WAKEUP_4);   // 可选：禁用部分 WAKEUP 引脚（复用为GPIO时需要）
 * 修    改：
 ***************************************************************************/
void gpio_out_init() {
    #if IO_SWITCH_OUTPUT_ENABLE
    liot_trace("IO_SWITCH_OUTPUT_ENABLE is enabled, initialize switch io...");
    init_switch_io();
    #elif  IO_MOTOR_OUTPUT_ENABLE
    liot_trace("IO_MOTOR_OUTPUT_ENABLE is enabled, initialize motor io...");
    init_motor_io();
    #endif
}


 /***************************************************************************
 * 名    称：net_led_init
 * 功    能：网络LED指示灯初始化
 * 说    明：
 *   - 用结构体统一管理LED引脚参数，便于维护和扩展。
 *   - 先设置引脚复选功能，再初始化为输出低电平。
 *   - 返回GPIO初始化结果，便于上层判断是否成功。
 * 修    改：24-08-11 优化为表驱动方式
 ***************************************************************************/
liot_gpio_errcode_e net_led_init() {
    struct {
        uint8_t pin;
        uint8_t func;
        uint32_t gpio;
    } led_cfg = {NET_LED_PIN, NET_LED_FUNC, NET_LED_GPIO};

    liot_gpio_errcode_e gpio_init_result;
    gpio_init_result = liot_pin_set_func(led_cfg.pin, led_cfg.func);
    if(gpio_init_result != LIOT_GPIO_SUCCESS){
        return gpio_init_result;
    }

    gpio_init_result = liot_gpio_init(led_cfg.gpio, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);
    if(gpio_init_result != LIOT_GPIO_SUCCESS){
        return gpio_init_result;
    }

    return LIOT_GPIO_SUCCESS;
}


/***************************************************************************
 * 名    称：init_switch_io
 * 功    能：io输出初始化
 * 说    明：
 * 修    改：24-08-11 增加4控4输出引脚初始化
 ***************************************************************************/
void init_switch_io(){
    liot_pin_set_func(16, 0);            // Default initialization for GPIO_OUT_1
    liot_gpio_init(GPIO_OUT_1, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

#if MAG_LATCH_PULSE_OUTPUT_ENABLE
    liot_pin_set_func(20, 0);            // Magnetic pulse output
    liot_gpio_init(GPIO_OUT_1_ON, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

    liot_pin_set_func(25, 0);            // Magnetic pulse output
    liot_gpio_init(GPIO_OUT_1_OFF, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);
#endif

#if FOUR_CHANNEL_OUTPUT_ENABLE
    liot_pin_set_func(5, 0);            // 4-channel output
    liot_gpio_init(GPIO_OUT_2, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

    liot_pin_set_func(63, 0);
    liot_gpio_init(GPIO_OUT_3, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

    liot_pin_set_func(101, 0);
    liot_gpio_init(GPIO_OUT_4, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);
#endif
}

//-----------------------------------MOTOR-----------------------------------
//-----------------------------------MOTOR-----------------------------------
//-----------------------------------MOTOR-----------------------------------

/***************************************************************************
 * 名    称：init_motor_io
 * 功    能：io输出初始化
 * 说    明：
 * 修    改：
 ***************************************************************************/
void init_motor_io(){
    liot_pin_set_func(5, 0);            // Initialize GPIO_MOTOR_FORWARD
    liot_gpio_init(GPIO_MOTOR_FORWARD, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

    liot_pin_set_func(16, 0);           // Initialize GPIO_MOTOR_REVERSE
    liot_gpio_init(GPIO_MOTOR_REVERSE, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

    liot_pin_set_func(20, 0);           // Initialize GPIO_MOTOR_FORWARD_LED
    liot_gpio_init(GPIO_MOTOR_FORWARD_LED, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);

    liot_pin_set_func(25, 0);           // Initialize GPIO_MOTOR_REVERSE_LED
    liot_gpio_init(GPIO_MOTOR_REVERSE_LED, LIOT_GPIO_OUTPUT, LIOT_FORCE_PULL_NONE, LIOT_LVL_LOW);
}



