#ifndef __DRV_APE_H__
#define __DRV_APE_H__

#include "liot_dev.h"
#include "liot_nw.h"

#include "drv_cloud_common.h"      


extern char aep_data_alarm[];
extern char aep_data_send_poweron[];
extern char aep_data_send_all[];

e_CAT1_REG_STATUS aep_mqtt_reg();
void aep_mqtt_reg_identity_strcat();
void aep_mqtt_reg_option_init();


void aep_init_suc_datasend(e_COMMON_UP_COMMAND initCmd);
void aep_mqtt_deinit();


uint8_t aep_datasend(e_COMMON_UP_COMMAND cmd);
void aep_datasend_fillproto(e_COMMON_UP_COMMAND com);
void aep_datasend_fillproto_poweron();

uint8_t aep_datasend_ack(e_IOT_DOWN_COMMAND cmd);
uint8_t aep_datasend_fault(e_FAULT_TYPE cmd);


void aep_update_net_signal();


#endif

