#ifndef __TASK_KEY_H__
#define __TASK_KEY_H__

#include "liot_gpio.h"

#define KEY_DEBOUNCE_30MS   3           // 10mS检测周期
#define KEY_DEBOUNCE_160MS  16          // 10mS检测周期


// typedef enum {
//     en_switch_key1=1,  	
//     en_switch_key2=2,
//     en_switch_key3=3,
//     en_switch_key4=4,	
//     en_switch_key1_ON=5,  		
//     en_switch_key1_OFF=6, 
// } e_switch_key;

typedef enum{   // 平台下行数据，设备确认结果
    en_SWITCH_KEY_CONFIRM_SUC = 0,                // 按键确认成功
    en_SWITCH_KEY_CONFIRM_FAIL = 1,               // 按键确认失败
    en_SWITCH_KEY_CONFIRM_FAIL_UART_FEEDBACK = 2, // 按键确认失败，UART没有反馈ACK
} e_SWITCH_KEY_CONFIRM_RESULT;



void liot_create_key_task(void);
void Key_Task(void *pvParameters);
void key_switch_press_check(void);
uint8_t key_switch_debounce_common(liot_gpio_num_e gpio, uint8_t debounce_count);
e_SWITCH_KEY_CONFIRM_RESULT KeySwich_ConfirmUART_Feedback();
void KeySwich_Excute(e_SWITCH_KEY_CONFIRM_RESULT confirm_result);
uint8_t key_switch_press_check_key1(void);
uint8_t key_switch_press_check_key1_on_off(void);
uint8_t key_switch_press_check_key2(void);
uint8_t key_switch_press_check_key3(void);
uint8_t key_switch_press_check_key4(void);



#endif /* __TASK_KEY_H__ */