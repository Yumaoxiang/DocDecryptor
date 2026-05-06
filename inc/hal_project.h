#ifndef __HAL_PROJECT_H__
#define __HAL_PROJECT_H__

#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "stdbool.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_type.h"
#include "liot_power.h"
#include "slpman.h"
#include "liot_adc.h"
#include "reset.h"

#include "hal_adc.h"
#include "hal_key.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hal_flash.h"
#include "drv_aep.h"
#include "drv_onenet.h"
#include "drv_fota.h"
#include "drv_cloud_common.h"
#include "drv_uart_protocol.h"
#include "task_key.h"
#include "task_fota.h"
#include "task_uart.h"
#include "task_aep.h"
#include "task_onenet.h"


#define MODULE_NT26KCNB00NNA            0   // NT26KCNB00NNA模块
#define MODULE_NT26KCNB00NNC            1   // NT26KCNB00NNC模块
#define MODULE_NT26K0B1                 0   // NT26K0B1模块
#define MODULE_NT21KCNA                 0   // NT21模块

#define PRODUCT_SWITCH                  1   // 开关产品
#define PRODUCT_MOTOR                   0   // 电机产品

#define CT_AEP                          0   // 连接电信AEP
#define CMCC_ONENET                     1   // 连接移动Onenet

#define MEM_STATE_MCU_FLASH             0   // 状态记忆存储在MCU中
#define MEM_STATE_CAT1_FLASH            1   // 状态记忆存储在Cat.1模块中

#define UART_COMM_ENABLE                0   // 使能 UART 通信
#define UART_PROTOCOL_FORMAT_ASCII      0   // 使用 ASCII 协议格式 (1=启用，0=禁用) [与 HEX 互斥]
#define UART_PROTOCOL_FORMAT_HEX        0   // 使用 Hex 协议格式 (1=启用，0=禁用) [与 ASCII 互斥]

#define IO_SWITCH_OUTPUT_ENABLE         1   // 使能开关 IO 输出功能
#define MAG_LATCH_PULSE_OUTPUT_ENABLE   1   // 使能磁保持脉冲输出功能
#define FOUR_CHANNEL_OUTPUT_ENABLE      0   // 使能四路输出功能（仅开关产品）
#define IO_MOTOR_OUTPUT_ENABLE          0   // 使能电机IO输出功能

#define KEY_SWITCH_ENABLE               1   // 使能按键输入功能
#define KEY_SWITCH_1_ON_OFF_ENABLE      1   // 使能常开、常关按键功能
#define KEY_SWITCH_2_3_4_ENABLE         0   // 使能四路按键功能

#define DEVICE_OFFLINE_RESET_ENABLE     1   // 使能模组自行复位功能



/* 根据不同模块给出不同的 GPIO/功能定义 */
#if   (MODULE_NT26K0B1 || MODULE_NT26KCNB00NNC || MODULE_NT26KCNB00NNA)  // NT26KCNB00NNC 模块的 GPIO 映射
#define NET_LED_GPIO        LIOT_GPIO_15
#define NET_LED_PIN         97
#define NET_LED_FUNC        0

#elif MODULE_NT21KCNA                                //NT21KCNA 模块的 GPIO 映射
#define NET_LED_GPIO        LIOT_GPIO_10
#define NET_LED_PIN         50
#define NET_LED_FUNC        1
#endif



#endif

