#ifndef __HAL_KEY_H__
#define __HAL_KEY_H__

typedef struct {
    uint8_t   info1;     
    uint8_t   info1_on;   
    uint8_t   info1_off;    
    uint8_t   info2;
    uint8_t   info3;
    uint8_t   info4;
} key_input_switch;

#define SWITCH_KEY_1               LIOT_GPIO_20
#define SWITCH_KEY_1_ON            LIOT_GPIO_18
#define SWITCH_KEY_1_OFF           LIOT_GPIO_19
#define SWITCH_KEY_2               LIOT_GPIO_17
#define SWITCH_KEY_3               LIOT_GPIO_6
#define SWITCH_KEY_4               LIOT_GPIO_7


extern liot_queue_t key_switch_queue;

void key_switch_init(void);
void key_switch_disable(void);
void key_switch_enable(void);
void key_switch_press_check(void);

#define KEY_MOTOR_DEBOUNCE_30MS   3                     // 10mS检测周期
#define KEY_MOTOR_DEBOUNCE_50MS   5                     // 10mS检测周期

#define MOTOR_FORWORD_INPUT             LIOT_GPIO_18
#define MOTOR_FORWARD_LIMIT_INPUT       LIOT_GPIO_19
#define MOTOR_STOP_INPUT                LIOT_GPIO_20
#define MOTOR_REVERSE_INPUT             LIOT_GPIO_6
#define MOTOR_REVERSE_LIMIT_INPUT       LIOT_GPIO_7



#endif /* __HAL_KEY_H__ */