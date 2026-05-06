#ifndef __HAL_GPIO_H__
#define __HAL_GPIO_H__

#include "liot_gpio.h"
#include "hal_project.h"





#define LED_NET_ON          liot_gpio_set_level(NET_LED_GPIO,LIOT_LVL_HIGH)
#define LED_NET_OFF         liot_gpio_set_level(NET_LED_GPIO,LIOT_LVL_LOW)
#define LED_NET_ON_10MS     LED_NET_ON;\
                            liot_rtos_task_sleep_ms(10);\
                            LED_NET_OFF
#define LED_NET_ON_1S       LED_NET_ON;\
                            liot_rtos_task_sleep_ms(1000);\
                            LED_NET_OFF



#define GPIO_OUT_1          LIOT_GPIO_14
#define GPIO_OUT_1_ON       LIOT_GPIO_11
#define GPIO_OUT_1_OFF      LIOT_GPIO_12
#define GPIO_OUT_2          LIOT_GPIO_0
#define GPIO_OUT_3          LIOT_GPIO_1
#define GPIO_OUT_4          LIOT_GPIO_13

#define GPIO_MOTOR_FORWARD      LIOT_GPIO_0
#define GPIO_MOTOR_REVERSE      LIOT_GPIO_14
#define GPIO_MOTOR_FORWARD_LED  LIOT_GPIO_11
#define GPIO_MOTOR_REVERSE_LED  LIOT_GPIO_12




#define IO_OUT_1_H          liot_gpio_set_level(GPIO_OUT_1,LIOT_LVL_HIGH)
#define IO_OUT_1_L          liot_gpio_set_level(GPIO_OUT_1,LIOT_LVL_LOW)

#define IO_OUT_1_ON_H       liot_gpio_set_level(GPIO_OUT_1_ON,LIOT_LVL_HIGH)
#define IO_OUT_1_ON_L       liot_gpio_set_level(GPIO_OUT_1_ON,LIOT_LVL_LOW)
#define IO_OUT_1_ON_H_100MS IO_OUT_1_ON_H;\
                            liot_rtos_task_sleep_ms(100);\
                            IO_OUT_1_ON_L

#define IO_OUT_1_OFF_H      liot_gpio_set_level(GPIO_OUT_1_OFF,LIOT_LVL_HIGH)
#define IO_OUT_1_OFF_L      liot_gpio_set_level(GPIO_OUT_1_OFF,LIOT_LVL_LOW)
#define IO_OUT_1_OFF_H_100MS IO_OUT_1_OFF_H;\
                             liot_rtos_task_sleep_ms(100);\
                             IO_OUT_1_OFF_L


#define IO_OUT_2_H          liot_gpio_set_level(GPIO_OUT_2,LIOT_LVL_HIGH)
#define IO_OUT_2_L          liot_gpio_set_level(GPIO_OUT_2,LIOT_LVL_LOW)


#define IO_OUT_3_H          liot_gpio_set_level(GPIO_OUT_3,LIOT_LVL_HIGH)
#define IO_OUT_3_L          liot_gpio_set_level(GPIO_OUT_3,LIOT_LVL_LOW)


#define IO_OUT_4_H          liot_gpio_set_level(GPIO_OUT_4,LIOT_LVL_HIGH)
#define IO_OUT_4_L          liot_gpio_set_level(GPIO_OUT_4,LIOT_LVL_LOW)

//////////////////////////////////////////////////////////////////////////

#define MOTOR_FORWARD_OUT_H     liot_gpio_set_level(GPIO_MOTOR_FORWARD,LIOT_LVL_HIGH)
#define MOTOR_FORWARD_OUT_L     liot_gpio_set_level(GPIO_MOTOR_FORWARD,LIOT_LVL_LOW)

#define MOTOR_FORWARD_LED_OUT_H liot_gpio_set_level(GPIO_MOTOR_FORWARD_LED,LIOT_LVL_HIGH)
#define MOTOR_FORWARD_LED_OUT_L liot_gpio_set_level(GPIO_MOTOR_FORWARD_LED,LIOT_LVL_LOW)

#define MOTOR_REVERSE_OUT_H     liot_gpio_set_level(GPIO_MOTOR_REVERSE,LIOT_LVL_HIGH)
#define MOTOR_REVERSE_OUT_L     liot_gpio_set_level(GPIO_MOTOR_REVERSE,LIOT_LVL_LOW)

#define MOTOR_REVERSE_LED_OUT_H liot_gpio_set_level(GPIO_MOTOR_REVERSE_LED,LIOT_LVL_HIGH)
#define MOTOR_REVERSE_LED_OUT_L liot_gpio_set_level(GPIO_MOTOR_REVERSE_LED,LIOT_LVL_LOW)



void gpio_out_init();
void init_switch_io();
void init_motor_io();
liot_gpio_errcode_e net_led_init();



#endif

