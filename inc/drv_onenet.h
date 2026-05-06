#ifndef __DRV_ONENET_H__
#define __DRV_ONENET_H__


extern char onenet_data_poweron[];
extern char onenet_data_common[];


void onenet_mqtt_reg_option_init();
e_CAT1_REG_STATUS onenet_mqtt_sub_topic();
e_CAT1_REG_STATUS onenet_mqtt_reg();
void onenet_mqtt_deinit();

uint8_t onenet_datasend(e_COMMON_UP_COMMAND cmd);
void onenet_datasend_fillproto_poweron();
void onenet_init_suc_datasend(e_COMMON_UP_COMMAND initCmd);
void onenet_datasend_fillproto_common(e_COMMON_UP_COMMAND com);
void onenet_update_net_signal();
void onenet_update_sub_topic_info();


uint8_t onenet_datasend_fault(e_FAULT_TYPE cmd);


uint8_t onenet_datasend_ack(e_IOT_DOWN_COMMAND cmd);
uint16_t parse_onenet_id(const char *payload);
void parse_onenet_params(const char *payload, char *output);

#endif
