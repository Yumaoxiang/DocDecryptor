/***************************************************************************
 * @File Name: drv_net.c
 * @Author : ymx
 * @Version : 1.0
 * @Creat Date : 2025-11-26
 * @copyright Copyright (c) 2024 Lierda Science & Technology Group Co., Ltd.
 * 说    明： 网络驱动相关接口实现，AEP与OneNet前期驻网相关功能
 * 
***************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "lierda_log.h"
#include "liot_dev.h"
#include "liot_os.h"
#include "liot_sim.h"
#include "liot_datacall.h"
#include "liot_type.h"
#include "sockets.h"
#include "liot_fs_api.h"
#include "liot_mqtt_client.h"
#include "ps_lib_api.h"
#include "liot_sim.h"

#include "drv_cloud_common.h"
#include "hal_project.h"



e_NET_LED_STATUS net_led = en_STATUS_OFF;

cat1_info_t cat1;
cat1_reboot_t cat1reboot;
motor_info_t motor = {MODE_IDLE_HOLD};  // 电机状态，初始化为停止状态

liot_queue_t platform_queue = NULL; // 名称含义：平台相关消息队列（包含 AEP/OneNet/其他来源）

// onenet与aep共用
char iot_data_ack[]="{\"cmd\":\"RemoteAck\",\"data\":\"Succ\",\"msgID\":\"000000\",\"workmode\":\"Swt\"}";

/************************************************************************************
 * 名    称：mqtt_connect_result_cb
 * 功    能：mqtt连接回调函数，可确认是否连接成功
 * 入口参数：client mqtt客户端句柄，初始值必须为0
 * 说    明：调用liot_mqtt_connect之后，主线程等待信号量，进入回调函数会释放信号量
************************************************************************************/
void mqtt_connect_result_cb(liot_mqtt_client_t *client, void *arg, int status){
    // liot_trace("connect cb: %d", status);
    if (status == 0){
        cat1.reg_flag = 1;
    }
}

/************************************************************************************
 * 名    称：mqtt_state_exception_cb
 * 功    能：会话连接异常断开的回调函数
 * 入口参数：client mqtt客户端句柄，初始值必须为0
 * 说    明：其他位置根据mqtt_connecte=0做异常处理
************************************************************************************/
void mqtt_state_exception_cb(liot_mqtt_client_t *client){
    liot_trace("mqtt exception!!!");
    cat1.reg_flag = 0;
}

/************************************************************************************
 * 名    称：mqtt_requst_result_cb
 * 功    能：模组发布消息时，对应回调函数 (QOS=1时，是否在这里确认的？)
 * 入口参数：
 * 说    明：err的返回值，0为成功，其他失败，具体参考"liot_mqtt_error_code_e"
 *          订阅回调与post回调一致，所以在mqtt_requst_result_cb中同时处理2个变量
 * 修    改：24-05-07 需要判断返回值是否为0，然后做逻辑处理
************************************************************************************/
void mqtt_requst_result_cb(liot_mqtt_client_t *client, void *arg, int err){
    if(err == 0){
        cat1.sub_flag = 1; 
        cat1.reg_flag = 1; 
        cat1.pub_suc_flag = 1;
    }
}

/************************************************************************************
 * 名    称：mqtt_disconnect_result_cb
 * 功    能：向服务器发送DISCONNECT请求，断开连接
 * 入口参数：
 * 说    明：后期根据确认码，加入逻辑判断
************************************************************************************/
void mqtt_disconnect_result_cb(liot_mqtt_client_t *client, void *arg, int err){
    // liot_trace("disconnect cb: %d", err);
    if(err == 0){
        cat1.deinitSucFlag = 1;
    }
}

/************************************************************************************
 * 名    称：mqtt_inpub_data_cb
 * 功    能：服务器发布消息时，模组接受回调函数
 * 入口参数：pkt_id待确认
 * 说    明：后期使用变量，在task中打印，禁止在callback中打印
************************************************************************************/
void mqtt_inpub_data_cb(liot_mqtt_client_t *client, void *arg, int pkt_id, const char *topic, const unsigned char *payload, unsigned short payload_len){
    memset(cat1.rcv.buf,0,RCV_BUF_SIZE_512);
    if(payload_len<RCV_BUF_SIZE_512)
		memcpy(cat1.rcv.buf,payload,payload_len);
	else
		memcpy(cat1.rcv.buf,payload,RCV_BUF_SIZE_512);

    cat1.rcv.len = payload_len;
    cat1.rcv.flag = 1;

    comm_msg_t msg;
    msg.source = TRIGGER_SOURCE_PLATFORM;               // 表示平台触发
    msg.type = MSG_TYPE_PLATFORM_TRIGGER_DOWNLINK;      // 表示平台下发消息
    liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0);
}



//------------------------------------------------------------------------------------------------------------



/************************************************************************************
 * 名    称：cat1_get_modulefirmware_n_times
 * 功    能：获取模组型号 n 次
 * 入口参数：ntimes 尝试获取次数
 * 返回值  ：en_CAT1_GET_IMEI_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_modulefirmware_n_times(uint8_t ntimes){
    for(uint8_t i =0; i < ntimes; i++){                     // 连续获取n次
        if(cat1_get_modulefirmware() == en_CAT1_GET_MODULEFW_SUC){
            return en_CAT1_GET_MODULEFW_SUC;
        }
        liot_rtos_task_sleep_ms(500);                       // 尝试周期0.5S/次
        cat1_reg_led_check(en_CAT1_LED_MODULE_FW);          // 中间穿插LED闪烁
    }
    return en_CAT1_FAIL;
}

/************************************************************************************
 * 名    称：cat1_get_modulefirmware
 * 功    能：获取模组型号
 * 返回值  ：en_CAT1_GET_MODULEFW_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：该函数原计划防止固件烧录错误导致的接口不兼容问题，但目前仅能作为获取模组型号的函数使用
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_modulefirmware(void)
{
    char devinfo[64] = {0};

    memset(devinfo, 0, sizeof(devinfo));
    liot_dev_get_firmware_version(devinfo, sizeof(devinfo));
    liot_trace("MODULE & VERSION: %s", devinfo);

#if MODULE_NT26KCNB00NNA
    if (strstr(devinfo, "NT26KCNB00NNA") != NULL){
        return en_CAT1_GET_MODULEFW_SUC;
    }
    else{
        liot_trace("MATCH MODULE [NT26KCNB00NNA] FAIL");
        return en_CAT1_FAIL;
    }
#elif MODULE_NT26KCNB00NNC
    if (strstr(devinfo, "NT26KCNB00NNC") != NULL){
        return en_CAT1_GET_MODULEFW_SUC;
    }
    else{
        liot_trace("MATCH MODULE [NT26KCNB00NNC] FAIL");
        return en_CAT1_FAIL;
    }
#elif MODULE_NT26K0B1
    if (strstr(devinfo, "NT26K0B1") != NULL){
        return en_CAT1_GET_MODULEFW_SUC;
    }
    else{
        liot_trace("MATCH MODULE [NT26K0B1] FAIL");
        return en_CAT1_FAIL;
    }
#else
    liot_trace("MODULE FW CHECK: No module type defined!");
    return en_CAT1_FAIL;
#endif
}


/************************************************************************************
 * 名    称：cat1_get_imei_n_times_with_timeout
 * 功    能：获取模组IMEI n * 3次(内部持续执行3次)
 * 入口参数：ntimes 尝试获取次数
 * 返回值  ：en_CAT1_GET_IMEI_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_imei_n_times_with_timeout(uint8_t ntimes){
    if(cat1_get_imei_n_times(ntimes) == en_CAT1_GET_IMEI_SUC){  // 尝试读取n次
        return en_CAT1_SUC;
    }

    if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_MIN, 5, enCAT1_IMEI) != en_CAT1_CFUN_SUC){
        return en_CAT1_FAIL;
    }

    if(cat1_get_imei_n_times(ntimes) == en_CAT1_GET_IMEI_SUC){  // 再次尝试读取n次
        return en_CAT1_SUC;
    }

    if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_FULL, 5, enCAT1_IMEI) != en_CAT1_CFUN_SUC){
        return en_CAT1_FAIL;
    }
    
    if(cat1_get_imei_n_times(ntimes) == en_CAT1_GET_IMEI_SUC){  // 再次尝试读取n次
        return en_CAT1_SUC;
    }

    return en_CAT1_FAIL;
}

/************************************************************************************
 * 名    称：cat1_get_imei_n_times
 * 功    能：获取模组IMEI n次
 * 入口参数：ntimes 尝试获取次数
 * 返回值  ：en_CAT1_GET_IMEI_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：上层函数中做异常处理，如尝试CFUN或者退出操作
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_imei_n_times(uint8_t ntimes){
    for(uint8_t i =0; i < ntimes; i++){                     // 连续获取n次
        if(cat1_get_imei() == en_CAT1_GET_IMEI_SUC){
            return en_CAT1_GET_IMEI_SUC;
        }
        liot_rtos_task_sleep_ms(500);                       // 尝试周期0.5S/次
        cat1_reg_led_check(en_CAT1_LED_IMEI);               // 中间穿插LED闪烁
    }
    return en_CAT1_FAIL;
}

/************************************************************************************
 * 名    称：cat1_get_imei
 * 功    能：获取模组IMEI
 * 返回值  ：en_CAT1_GET_IMEI_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_imei(){
    liot_errcode_dev_e ret;

    memset(cat1.imei, 0, sizeof(cat1.imei));        // 获取之前，先清0
    ret = liot_dev_get_imei(cat1.imei, 16, 0);      // 获取IMEI并存储到cat1.imei
    if(ret != LIOT_DEV_SUCCESS){                    
        liot_trace("get imei fail: %s", ret);
        return en_CAT1_FAIL;
    }
    liot_trace("IMEI: %s", cat1.imei);

    return en_CAT1_GET_IMEI_SUC; 
}


/************************************************************************************
 * 名    称：cat1_get_iccid_n_times_with_timeout
 * 功    能：获取模组ICCID n * 2次(内部持续执行3次)
 * 入口参数：ntimes 尝试获取次数
 * 返回值  ：en_CAT1_GET_ICCID_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_iccid_n_times_with_timeout(uint8_t ntimes){
    if(cat1_get_iccid_n_times(ntimes) == en_CAT1_GET_ICCID_SUC){  // 尝试读取n次
        return en_CAT1_SUC;
    }

    if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_MIN, 5, enCAT1_ICCID) != en_CAT1_CFUN_SUC){
        return en_CAT1_FAIL;
    }

    if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_FULL, 5, enCAT1_ICCID) != en_CAT1_CFUN_SUC){
        return en_CAT1_FAIL;
    }
    
    if(cat1_get_iccid_n_times(ntimes) == en_CAT1_GET_ICCID_SUC){  // 再次尝试读取n次
        return en_CAT1_SUC;
    }

    return en_CAT1_FAIL;
}

/************************************************************************************
 * 名    称：cat1_get_iccid_n_times
 * 功    能：获取模组ICCID n次
 * 入口参数：ntimes 尝试获取次数
 * 返回值  ：en_CAT1_GET_ICCID_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：上层函数中做异常处理，如尝试CFUN或者退出操作
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_iccid_n_times(uint8_t ntimes){
    for(uint8_t i =0; i < ntimes; i++){                     // 连续获取n次
        if(cat1_get_iccid() == en_CAT1_GET_ICCID_SUC){
            return en_CAT1_GET_ICCID_SUC;
        }
        liot_rtos_task_sleep_ms(500);                       // 尝试周期0.5S/次
        cat1_reg_led_check(en_CAT1_LED_SIM_CARD);       // 中间穿插LED闪烁
    }
    return en_CAT1_FAIL;
}

/************************************************************************************
 * 名    称：cat1_get_iccid
 * 功    能：获取模组ICCID
 * 返回值  ：en_CAT1_GET_IMEI_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_iccid(){
    liot_sim_errcode_e ret;
    // char siminfo[64] = {0};

    memset(cat1.iccid, 0, strlen(cat1.iccid));                      // 获取之前，先清0

    ret = liot_sim_get_iccid(0, cat1.iccid, sizeof(cat1.iccid));    // 获取IMEI并存储到cat1.iccid
    if(ret != LIOT_SIM_SUCCESS){                    
        liot_trace("get iccid fail: ret:0x%x", ret);                // 注意：打印是%x，如果是%s会复位
        return en_CAT1_FAIL;
    }
    liot_trace("ICCID: %s", cat1.iccid);

    return en_CAT1_GET_ICCID_SUC;
}


/************************************************************************************
 * 名    称：cat1_net_acess_n_times_with_timeout
 * 功    能：查询驻网 n * 2次(内部持续执行2次)，每次间隔0.5S
 * 入口参数：ntimes 尝试获取次数
 * 返回值  ：en_CAT1_NET_REG_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：
************************************************************************************/
e_CAT1_REG_STATUS cat1_net_acess_n_times_with_timeout(uint8_t ntimes){
    if(cat1_net_acess_n_times(ntimes) == en_CAT1_NET_REG_SUC){  // 尝试读取n次
        return en_CAT1_SUC;
    }

    if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_MIN, 5, enCAT1_NET_ACCESS) != en_CAT1_CFUN_SUC){
        return en_CAT1_FAIL;
    }

    if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_FULL, 5, enCAT1_NET_ACCESS) != en_CAT1_CFUN_SUC){
        return en_CAT1_FAIL;
    }
    
    if(cat1_net_acess_n_times(ntimes) == en_CAT1_NET_REG_SUC){  // 再次尝试读取n次
        return en_CAT1_SUC;
    }

    return en_CAT1_FAIL;
}

/************************************************************************************
 * 名    称：cat1_net_acess_n_times
 * 功    能：确认网络驻留n次，每次间隔0.5S
 * 入口参数：ntimes 查询次数
 * 返回值  ：en_CAT1_NET_REG_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：上层函数中做异常处理，如尝试CFUN或者退出操作
 * 修    改：25-09-22 底层函数使用liot_network_register_cereg_get替代liot_network_register_get
************************************************************************************/
e_CAT1_REG_STATUS cat1_net_acess_n_times(uint8_t ntimes){
    for(uint8_t i =0; i < ntimes; i++){          
        if(LIOT_DATACALL_SUCCESS == liot_network_register_cereg_get(LIOT_SIM_1)){
            liot_trace("net access success");
            return en_CAT1_NET_REG_SUC;
        }
        liot_trace("net access %d times", i);           // 周期打印
        liot_rtos_task_sleep_ms(500);                   // 尝试周期0.5S/次
        cat1_reg_led_check(en_CAT1_LED_NET);            // 中间穿插LED闪烁

        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd3_push_net_status();                // 通过UART发送当前注册状态
        #endif
    }
    return en_CAT1_FAIL;
}



/************************************************************************************
 * 名    称：cat1_get_net_signal
 * 功    能：获取网络信号值
 * 返回值  ：en_CAT1_GET_SIG_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：Lierda封装的函数有问题，所以使用原厂appGetECBCInfoSync获取网络信号
************************************************************************************/
e_CAT1_REG_STATUS cat1_get_net_signal(){
    BasicCellListInfo bcListInfo = {0};

    CmsRetId rtn = appGetECBCInfoSync(&bcListInfo);
    if (rtn != CMS_RET_SUCC) {
        liot_trace("appGetECBCInfoSync fail: %d", rtn);
        return en_CAT1_FAIL;
    }
    liot_trace("rsrp:%d, snr:%d", bcListInfo.sCellInfo.rsrp, bcListInfo.sCellInfo.snr);
    if (bcListInfo.sCellInfo.rsrp < -156 || bcListInfo.sCellInfo.rsrp > -44) {    // Check if rsrp is within range
        liot_trace("Error: RSRP out of range!\n");
        return en_CAT1_FAIL;
    }
    if (bcListInfo.sCellInfo.snr < -30 || bcListInfo.sCellInfo.snr > 40) {       // Check if snr is within range
        liot_trace("Error: SNR out of range!\n");
        return en_CAT1_FAIL;
    }

    memset(cat1.netSignal, 0, sizeof(cat1.netSignal));     
    sprintf(cat1.netSignal, "%04d %03d",  bcListInfo.sCellInfo.rsrp, bcListInfo.sCellInfo.snr);
    liot_trace("cat1_get_net_signal() cat1.netSignal is: %s", cat1.netSignal);

    return en_CAT1_GET_SIG_SUC;
}


/************************************************************************************
 * 名    称：cat_reg_aep_led_check
 * 功    能：平台注册时LED闪烁
 * 入口参数：mode 当前AEP模式，匹配不同的闪烁方式
 * 说    明：定时器周期1S/次，cat1.net_led_cnt自增，task函数中延时不建议超过500mS(否则LED周期不准)
 
 BEFORE_SIM：读卡之前所有操作，闪烁周期2S：1S亮/1S灭
 SIM_CARD：读卡，闪烁周期2S：10mS脉冲
 NET：网络注册，闪烁方式1S：10mS脉冲 * 1
 AEP_REG：平台注册，闪烁方式1S：10mS脉冲 * 2
 * 修    改：25-10-06 V5.0之后SIM卡读卡时，闪烁周期2S--> 3S，考虑到SIM卡增加平台匹配校验，便于用户观察
************************************************************************************/
#define LED_MODULE_FW_CYCLE_2S      2
#define LED_IMEI_FAULT_CYCLE_1S     1
#define LED_SIM_CYCLE_3S            3
#define LED_NET_REG_CYCLE_1S        1
#define LED_AEP_REG_FAIL_CYCLE_1S   1
void cat1_reg_led_check(e_CAT1_REG_AEP_LED_MODE mode){
    switch(mode){
    // 读取模组型号，防止NNC、NNA差异烧录读，异常闪烁周期4S：2S亮/2S灭
    case en_CAT1_LED_MODULE_FW:                         
        if(cat1.net_led_cnt >= LED_MODULE_FW_CYCLE_2S){    
            cat1.net_led_cnt = 0;
            if(net_led == en_STATUS_OFF){
                LED_NET_ON;
                net_led = en_STATUS_ON;
            }else{
                LED_NET_OFF;
                net_led = en_STATUS_OFF;
            }
        }
    break;

    // 读取IMEI，异常时闪烁周期2S：1S亮/1S灭
    case en_CAT1_LED_IMEI:                            
        if(cat1.net_led_cnt >= LED_IMEI_FAULT_CYCLE_1S){    
            cat1.net_led_cnt = 0;
            if(net_led == en_STATUS_OFF){
                LED_NET_ON;
                net_led = en_STATUS_ON;
            }else{
                LED_NET_OFF;
                net_led = en_STATUS_OFF;
            }
        }
    break;

    case en_CAT1_LED_SIM_CARD:                              // 读卡，闪烁周期2S：10mS脉冲 * 1
        if(cat1.net_led_cnt >= LED_SIM_CYCLE_3S){       
            cat1.net_led_cnt = 0;
            LED_NET_ON_10MS;
        }
    break;

    case en_CAT1_LED_SIM_MATCH:                             // 读卡，闪烁周期2S：10mS脉冲 * 2
        if(cat1.net_led_cnt >= LED_SIM_CYCLE_3S){       
            cat1.net_led_cnt = 0;
            LED_NET_ON_10MS;
            liot_rtos_task_sleep_ms(200);
            LED_NET_ON_10MS;
        }
    break;

    case en_CAT1_LED_NET:                                   //网络注册，闪烁方式1S：10mS脉冲 * 1
        if(cat1.net_led_cnt >= LED_NET_REG_CYCLE_1S){       
            cat1.net_led_cnt = 0;
            LED_NET_ON_10MS;
        }
    break;

    case en_CAT1_LED_CLOUD_REG:                               // 平台注册，闪烁方式1S：10mS脉冲 * 2
        if(cat1.net_led_cnt >= LED_AEP_REG_FAIL_CYCLE_1S){       
            cat1.net_led_cnt = 0;
            LED_NET_ON_10MS;
            liot_rtos_task_sleep_ms(100);
            LED_NET_ON_10MS;
        }
    break;

    default: break;
    }
}


/************************************************************************************
 * 名    称：cat1_set_cfun_ntimes
 * 功    能：设置CFUN N次
 * 入口参数：mode CFUN模式
 *          ntimes 尝试获取次数
 *          status 当前AEP注册位置，对应不同LED闪烁方式
 * 返回值  ：en_CAT1_GET_IMEI_SUC，获取成功
 *           其他参数，获取失败
 * 说    明：上层函数中做异常处理，如尝试CFUN或者退出操作
************************************************************************************/
e_CAT1_REG_STATUS cat1_set_cfun_ntimes(liot_dev_cfun_e mode, uint8_t ntimes, cat1_init_status_t status){
    uint8_t cfun = 0;
    
    for(uint8_t i = 0; i < ntimes; i++){            // 连续获取n次
        // 设置CFUN
        liot_dev_set_modem_fun(mode, 0, 0);         // 开始设置

        // 等待设置成功，每次等待100mS，最大等待2S
        for(uint8_t cnt = 0; cnt < 20; cnt++){      // 最大延时2S    
            //因为CFUN设置需要时间，所以中间插入LED闪烁，保证LED周期正确
            switch(status){  
            case enCAT1_IMEI:
                cat1_reg_led_check(en_CAT1_LED_IMEI);
            break;                       
            case enCAT1_ICCID:
                cat1_reg_led_check(en_CAT1_LED_SIM_CARD);
            break;
            case enCAT1_NET_ACCESS:
                cat1_reg_led_check(en_CAT1_LED_NET);
            break;
            case enCAT1_CLOUD_REG:
                cat1_reg_led_check(en_CAT1_LED_CLOUD_REG);
            break;
            default:
                cat1_reg_led_check(en_CAT1_LED_IMEI);
            break;
            }

            liot_rtos_task_sleep_ms(100);  
            liot_dev_get_modem_fun(&cfun, 0);
            liot_trace("get cfun: %d", cfun);
            if(cfun == mode){
                liot_trace("set cfun %d success", mode);
                return en_CAT1_CFUN_SUC;            
            }
            liot_trace("set cfun %d fail", mode);
        } 
    }
    
    return en_CAT1_FAIL;
}


//----------------------------------------------------------------------------------
//-------------------------上方注册相关，下方通信相关---------------------------------
//----------------------------------------------------------------------------------


/******************************************************************************
*名    称：Cat1_Init()
*功    能：Cat1初始化函数
*入口参数：initCmd 不同的命令，决定初始化成功后发送不同的数据
*返回值：0 失败    1 成功
*说    明： 初始化&心跳，注册成功后，最多发送N次
*******************************************************************************/
uint8 cat1_init(e_COMMON_UP_COMMAND initCmd){
    // Step1: 连续搜网2min
    LED_NET_OFF;                                    // 网络指示灯熄灭，用户指示
    cat1.init_cnt = 0;                              // 准备计时
    cat1.disconnect_1min_cnt = 0;                   // 如果失败，一定间隔后重新尝试注册   
    cat1.reg_status = enCAT1_MODULE_FW;             // 上电开始顺序执行读取IMEI不同功能
    while(((cat1.reg_status!=enCAT1_INIT_SUC)&&(cat1.init_cnt<TIME_CAT1_INIT_2MIN))){  //2min初始化
        cat1_init_register();
        liot_rtos_task_sleep_ms(10);                // 多线程防止死机

        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd3_push_net_status();            // 通过UART发送当前注册状态，便于MCU判断，这里要放在延时后面，保证状态更新后有10mS的时间差，避免MCU频繁收到状态更新
        #endif
    }

    // Step3: 判断是否初始化成功(未成功处理逻辑)
    if(((cat1.reg_status!=enCAT1_INIT_SUC)||(cat1.init_cnt>=TIME_CAT1_INIT_2MIN))){    //如果时间超出了，说明还是没有网络连接
        LED_NET_ON_1S;                              // LED常亮1S提示
        cat1.initSucFlag=0;                         // 初始化成功标志位，后面全局判断，如果没有入网，也就不用更新部分状态了
        cat1.disconnect_1min_cnt = 0;               // 失败，一定间隔后重新尝试注册 
        cat1.reg_status = enCAT1_INIT_FAIL;         // 初始化失败，UART读取时会根据该状态进行判断、返回
        liot_trace("Cat.1  Init  FAIL !");

        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd3_push("FAL");                  // 通过UART发送注册失败状态，便于MCU判断
        #endif

        return 0;
    }
    liot_trace("Cat.1 Init Success!");        
    cat1.cereg_access_time = enLONG_120S;           // 只要成功，下次搜网默认120S
    LED_NET_ON;                                     // 网络指示灯常亮
    cat1.initSucFlag=1;                             // 初始化成功标志位

    
    #if CT_AEP
    aep_init_suc_datasend(initCmd);
    #elif CMCC_ONENET
    onenet_init_suc_datasend(initCmd);
    onenet_update_sub_topic_info();                 // 初始化成功后，重新订阅下行主题，解决个别用户订阅失败导致无法下发问题
    #endif

    return 1;
}

/******************************************************************************
*名    称：cat1_init_register
*功    能：
*说    明：
*修    改：25-09-22 增加驻网失败清频，驻网查询使用liot_network_register_cereg_get
          25-10-07 增加cat1_get_modulefirmware_n_times，判断模组型号，但modle_config.cfg中与宏定义不一致也是固定输出NNC，原因未知
*******************************************************************************/
CiotSetFreqParams CiotFreqParams;

void cat1_init_register(){
    switch(cat1.reg_status){
    // 异常处理 CFUN0/ECFREQ3/CFUN1/ECRST

    // 正常上电执行位置
    case enCAT1_MODULE_FW:
        if(cat1_get_modulefirmware_n_times(5) != en_CAT1_GET_MODULEFW_SUC){   // 读取模组型号，如果失败不退出(需判断IMEI)，持续执行相当于提示
            liot_trace("MODULE Match Fail!");  
            break;                                                  // 不退出，长时间提示
        }
        liot_rtos_task_sleep_ms(20);                                // 多线程防止死机
        LED_NET_OFF;                                                // 下一个周期2S/次，防止切换时LED点亮2S
        cat1.reg_status = enCAT1_IMEI;                              // 准备读IMEI
        break;

    case enCAT1_IMEI:
        if(cat1_get_imei_n_times_with_timeout(5) != en_CAT1_SUC){   // 读取IMEI，如果失败不退出(需判断IMEI)，持续执行相当于提示
            liot_trace("IMEI read Fail!");  
            break;                                                  // 不退出，长时间提示
        }
        liot_rtos_task_sleep_ms(20);                                // 多线程防止死机
        LED_NET_OFF;                                                // 下一个周期2S/次，防止切换时LED点亮2S
        cat1.reg_status = enCAT1_ICCID;                             // 准备读卡
        break;
    
    case enCAT1_ICCID: 
        if(cat1_get_iccid_n_times_with_timeout(10) != en_CAT1_SUC){ // 读取ICCID，如果失败不退出，相当于提示
            liot_trace("case ICCID Fail!");  
            cat1.init_cnt = TIME_CAT1_INIT_2MIN;                    // 25-01-12 读卡最多10S，失败不在循环读取，便于其他功能调试
            break;                                                  // 不退出，长时间提示
        }
        // ICCID_OperatorCheck();                                      // 检查运营商平台连接是否与ICCID匹配 调试仿真
        liot_rtos_task_sleep_ms(50);                                // 多线程防止死机，同时防止发送UART状态时粘包
        cat1.reg_status = enCAT1_NET_ACCESS;                        // 准备查询驻网
        break;

    case enCAT1_NET_ACCESS: 
        if(cat1.cereg_access_time == enSHOUT_15S){                  // 断连后重新驻网频繁，只给5S时间
            if(cat1.ceregCfunFlag){                                 // 确认本次是否需要发送CFUN0/CFUN1
                if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_MIN, 5, enCAT1_NET_ACCESS) != en_CAT1_CFUN_SUC){
                    liot_trace("'enSHOUT_15S' CFUN0 fail"); 
                    break;
                }
                CiotFreqParams.mode=3;                              // 清频
                appSetCiotFreqSync(&CiotFreqParams);                // 清频
                liot_trace("clear freq finish"); 
                if(cat1_set_cfun_ntimes(LIOT_DEV_CFUN_FULL, 5, enCAT1_NET_ACCESS) != en_CAT1_CFUN_SUC){
                    liot_trace("'enSHOUT_15S' CFUN1 fail"); 
                    break;
                }
                liot_trace("'enSHOUT_15S' CFUN0 and CFUN1 success"); 
            }
            if(cat1_net_acess_n_times(30) != en_CAT1_NET_REG_SUC){  // 尝试读取15s
                liot_trace("case net access Fail!");  
                cat1.init_cnt = TIME_CAT1_INIT_2MIN;                // 退出本次初始化
                break;                                              
            }
        }else{ // 上电、正常数据重发、断连超过30min，给120S时间。 注意：上电不需要清频，只有心跳包弱信号切换中加入清频 25-09-22
            if(cat1_net_acess_n_times_with_timeout(120) != en_CAT1_SUC){// 驻网2min，失败建议退出
                liot_trace("case net access Fail!");  
                cat1.init_cnt = TIME_CAT1_INIT_2MIN;                // 退出本次初始化
                break;                                              
            }
        }
        liot_trace("AT+CEREG net access Success!");
        liot_rtos_task_sleep_ms(50);                                // 防止发送UART状态时粘包
        cat1.reg_status = enCAT1_CLOUD_REG;                         // 准备注册平台驻网
        break;

    case enCAT1_CLOUD_REG:                                                // 注册平台
        #if CT_AEP
        if(aep_mqtt_reg() != en_CAT1_NET_REG_SUC){
            liot_trace("cat1_init_register() register Fail!");  
            aep_mqtt_deinit();                                      // 去注册不在放在入口，因为失败后1min会再次尝试
            cat1.init_cnt = TIME_CAT1_INIT_2MIN;                    // 内部循环2次，一直无法驻网
            break;                                                  // 不退出，长时间提示
        }
        #elif CMCC_ONENET
        if(onenet_mqtt_reg() != en_CAT1_NET_REG_SUC){
            liot_trace("cat1_init_register() register Fail!");  
            onenet_mqtt_deinit();                                   // 去注册不在放在入口，因为失败后1min会再次尝试
            cat1.init_cnt = TIME_CAT1_INIT_2MIN;                    // 内部循环2次，一直无法驻网
            break;                                                  // 不退出，长时间提示
        }
        #endif
        liot_rtos_task_sleep_ms(30);                                // 防止发送UART状态时粘包
        cat1.reg_status = enCAT1_INIT_SUC;                          // 初始化成功
        break;
    
    default: 
        liot_trace("**** cat1_init_register() unrecognize state! ****");
        liot_rtos_task_sleep_ms(1000);                              // 多线程防止死机
        break;
    }
}


/************************************************************************************
 * 名    称：datasend_n_times_with_reconnect
 * 功    能：发送数据 n 次，失败则进行重连
 * 入口参数：cmd 发送的命令
 *          ntimes 发送数据最多ntimes次
 * 返回值  ：0-失败  1-成功收到平台ACK
 * 说    明：
************************************************************************************/
uint8_t datasend_n_times_with_reconnect(e_COMMON_UP_COMMAND cmd, uint8 ntimes){
    if(datasend_n_times(cmd, ntimes)){
        return 1;
    }

    if(cat1_init(cmd)){                     // 设备发送异常，重新初始化，内部会清除各种变量
        return 1;
    }

    LED_NET_OFF;                            // 网络指示灯熄灭，用户指示
    cat1.disconnect_1min_cnt = 0;           // 失败，重新计数，心跳包一定间隔后会重新尝试注册

    return 0;
}

uint8_t datasend_n_times_with_initcheck(e_COMMON_UP_COMMAND cmd, uint8 ntimes){
    if(!cat1.initSucFlag){                 // 离线
        liot_trace("aep init fail, no send to aep, return...");
        return 0;           
    }

    if(datasend_n_times(cmd, ntimes)){
        cat1.heart_cnt = 0;                // 数据发送成功，心跳包对应延迟
        return 1;
    }
    
    return 0;
}

/************************************************************************************
 * 名    称：aep_datasend_n_times
 * 功    能：发送数据 n 次
 * 入口参数：cmd 发送的命令
 *          ntimes 发送数据最多ntimes次
 * 返回值  ：0-失败  1-成功收到平台ACK
 * 说    明：
************************************************************************************/
uint8_t datasend_n_times(e_COMMON_UP_COMMAND cmd, uint8 ntimes){
    for(uint8_t i =1; i <= ntimes; i++){                     
        #if CT_AEP
        if(aep_datasend(cmd)){
            return 1;
        }
        #elif CMCC_ONENET
        if(onenet_datasend(cmd)){
            return 1;
        }
        #endif
        liot_trace("[info] datasend %d time Fail!", i);
    }
    return 0;
}


/*******************************************************************************
*名    称：replaceJson()
*功    能：替换特定字符
*入口参数：str -> 待替换的字符串
          srcstr -> 待替换的字符串前一级，对应key，比如"ver":"1.0.0"需要替换1.0.0，则查找ver
          desstr -> 替换后的字符串，对应value，比如"ver":"1.0.0"中1.0.0需要替换2.2(长度可以不一致)
*示    例：char aep_data_send_all[200]="{\"product\":\"3_3\",\"ver\":\"1.0.0\",\"cmd\":\"PowerOn\",\"msgID\":\"BBBBB1\",\"output\":\"0000\"}";
           replaceJson(aep_data_send_all, "cmd", "HeartBeat");
           replaceJson(aep_data_send_all, "ver", "1.0.1");
*修    改：23-05-13 入口参数增加msgID
*******************************************************************************/
void replaceJson(char *str, const char *srckey, const char *desvalue) {
    if ((str == NULL) || (srckey == NULL) || (desvalue == NULL)) {
        liot_trace("Error: replaceJson() Invalid input!");
        return;
    }
    // liot_trace("    1. original string[] is: %s", str);

    char *srcStart = strstr(str, srckey); 
    if(srcStart == NULL){
        liot_trace("Error: Key \"%s\" not found!", srckey);
        return;
    }

    char *cmdValueStart = srcStart + strlen(srckey) + 3;                // 输入"cmd"而不是输入\"cmd\":\"，所以起始位置要右移3个
    char *cmdValueEnd = strchr(cmdValueStart, '\"');
    if(cmdValueEnd == NULL){
        liot_trace("Error: End of key \"%s\" not found!", srckey);
        return;
    }  

    strncpy(cmdValueStart, desvalue, cmdValueEnd - cmdValueStart);
    // liot_trace("    2. replace string[] is: %s", str);
}

/*******************************************************************************
*名    称：extractJson()
*功    能：提取特定字符
*入口参数：str -> 待提取的字符串
          key -> 待替换的字符串前一级，比如"ver":"1.0.0"需要提取 1.0.0，则查找 ver
          value -> 提取后的字符串，比如"ver":"1.0.0"提取后的数据为"1.0.0"
          valuelen -> 目标缓冲区总大小（包含'\0'的空间）
*说    明：最大提取长度为 valuelen-1，预留 1 字节给'\0'
*示    例：extractJson(cat1.rcv.buf, "msgID", cat1.rcv.SerRandomID, sizeof(cat1.rcv.SerRandomID));
          extractJson(cat1.rcv.buf, "data", cat1.rcv.data, sizeof(cat1.rcv.data));
*修    改：
*******************************************************************************/
void extractJson(char *str, char *key, char *value, uint16 valuelen) {
    if ((str == NULL) || (key == NULL) || (value == NULL)) {
        liot_trace("Error: extractJson() Invalid input!\n");
        return;
    }
    
    char *keyStart = strstr(str, key); 
    if (keyStart == NULL) {
        liot_trace("Error: Key '%s' not found!\n", key);
        return;
    }

    char *valueStart = keyStart + strlen(key) + 3; // Move to the start of value
    char *valueEnd = strchr(valueStart, '\"');
    if (valueEnd == NULL) {
        liot_trace("Error: End of key '%s' not found!\n", key);
        return;
    }
    int length = valueEnd - valueStart;         // 计算字符串长度
    if(length >= valuelen){                     // 需要为'\0'预留空间，所以是>=
        liot_trace("Error: key&value size overflow\n");
        return;
    }

    strncpy(value, valueStart, length);
    value[length] = '\0';                       // 在有效长度位置添加字符串结束符

    liot_trace("Extracted key and value: '%s' --> '%s' ", key, value);
}


//---------------------------------通用Drv ↑-----------------------------------
//-----------------------------------------------------------------------------
//--------------------------------通用Task ↓-----------------------------------

void handle_key_trigger(comm_msg_t *msg){
    if(msg->source == TRIGGER_SOURCE_KEY){
        liot_trace("[platform] key trigger received, sending data to platform...");
        if(datasend_n_times_with_reconnect(en_CMD_LOCAL_SWITCH, 2)){
            cat1.heart_cnt = 0;                                                     // 数据发送成功，心跳包对应延迟
        }
    }
}



/*******************************************************************************
*名    称：handle_timer_trigger()
*功    能：处理定时器触发的事件，包括心跳和离线重连
*入口参数：msg - 指向 comm_msg_t 类型的指针，包含触发事件的相关信息
*说    明：根据消息类型调用相应的处理函数
*示    例：
*   comm_msg_t msg;
*   handle_timer_trigger(&msg);
*修    改：
*******************************************************************************/
void handle_timer_trigger(comm_msg_t *msg) {
    handle_heartbeat_message(msg);
    handle_reconnect_message(msg);
}


/*******************************************************************************
*名    称：handle_heartbeat_message()
*功    能：处理心跳消息
*入口参数：msg - 指向 comm_msg_t 类型的指针，包含心跳消息的相关信息
*说    明：处理心跳消息，重置相关计数器并更新设备参数
*示    例：
*   comm_msg_t msg;
*   handle_heartbeat_message(&msg);
*修    改：
*******************************************************************************/
void handle_heartbeat_message(comm_msg_t *msg) {
    if (msg->type == MSG_TYPE_TIMER_TRIGGER_HEARTBEAT) {
        // 心跳处理
        cat1.heart_cnt = 0;
        datasend_n_times_with_reconnect(en_CMD_HEART_BEAT, 2);
        adc_vbat_temp_get();                                // 更新vbat与temp参数，下次上发
        cat1.disconnect_30min_cnt = 0;                      // 每次心跳包清0，如果离线无法清0
        cat1.disconnect_1min_cnt = 0;                       // 每次心跳包
        cat1reboot.disconnect_cnt = 0;                      // 每次心跳包清0，如果离线无法清0
        cat1reboot.reboot_24hour_cnt = 0;                   // 每次心跳包清0，如果离线无法清0
    }
}

/*******************************************************************************
*名    称：handle_reconnect_message()
*功    能：处理离线重连消息
*入口参数：
*   msg - 指向 comm_msg_t 类型的指针，包含离线重连消息的相关信息
*说    明：处理离线重连逻辑，包括短搜、长搜和复位操作
*示    例：
*   comm_msg_t msg;
*   handle_reconnect_message(&msg);
*修    改：
*******************************************************************************/
void handle_reconnect_message(comm_msg_t *msg) {
    if (msg->type == MSG_TYPE_TIMER_TRIGGER_RECONNECT) {
        // 离线重连处理
        cat1.disconnect_1min_cnt = 0;

        // 超过30min，搜网时间恢复默认120S，每12小时，重新执行短搜
        cat1.disconnect_30min_cnt++;
        if (cat1.disconnect_30min_cnt < TIME_CAT1_RECONNECT_30MIN) {  // 30min内，短搜15S
            cat1.cereg_access_time = enSHOUT_15S;                   // 断线重连，每次搜网时间缩短
            cat1.ceregCfunFlag = !cat1.ceregCfunFlag;               // 每次短搜交替清频
        } else {
            cat1.cereg_access_time = enLONG_120S;                   // 失败30min，搜网默认120S，而且不在执行清频 
            liot_trace("dissconnect over 30min, reset search net time to 120s");

            if (cat1.disconnect_30min_cnt >= TIME_CAT1_RECONNECT_12HOUR) {
                cat1.disconnect_30min_cnt = 0;                      // 每12小时，重新执行短搜，短搜里面包含了清频
            }
        }

        cat1_init(en_CMD_HEART_BEAT);

        #if DEVICE_OFFLINE_RESET_ENABLE
        SysHeartBeat_Reboot();                                  // 判断是否需要执行复位操作
        #endif
    }
}

/*******************************************************************************
*名    称：SysHeartBeat_Reboot()
*功    能：如果超过1hour没有联网，则重启
*说    明：
*修    改：
*******************************************************************************/
void SysHeartBeat_Reboot(){
    liot_trace("SysHeartBeat_Reboot() CHECK");
    
    if(cat1.initSucFlag) return;                                                // 没有离线，则不复位

    #if PRODUCT_SWITCH
    // 开关产品：检查所有 4 路输出，只要有任何一路为 ON 就不复位 （这里即使1路也检查4路不影响结果）
    for(int i = 0; i < 4; i++){
        if(g_io.out[i].now_status == enIO_OUT_ON) return;                         // 任意一路输出为 ON，不复位
    }
    if(strcmp(flash_poweron.mode, "o_n") == 0) return;                            // 用户设置了"上电开机"，不复位
   
    #elif PRODUCT_MOTOR
    // 正反转电机：如果当前不是"stop"状态，不复位
    if((motor.currentState != MODE_IDLE_HOLD) && (motor.currentState != MODE_IDLE_JOG)) return;
    #endif

    if(cat1reboot.disconnect_cnt  < TIME_CAT1_DISCONNECT_24HOUR){
        liot_trace("disconnect_cnt %ld not reach 86400s, return!", cat1reboot.disconnect_cnt);
        return;   // 离线，但是没有超过24hour
    }
    cat1reboot.disconnect_cnt = 0;

    if(strcmp(flash_poweron.reboot, "reboot:3") == 0){
        liot_trace("reboot:3 return");
        return;                                                                 // 如果上电后提取的复位已经3次了，这里也不在 复位
    } 

    if((strcmp(flash_poweron.reboot, "reboot:1") == 0) || (strcmp(flash_poweron.reboot, "reboot:2") == 0)){ // 1小时之前已经复位了，后面必须24小时复位/次
        liot_trace("reboot_24hour_cnt = %ld", cat1reboot.reboot_24hour_cnt);
        if(cat1reboot.reboot_24hour_cnt  < TIME_CAT1_DISCONNECT_24HOUR){
            return;
        }
    }
    
    // "reboot:0"--1hour   "reboot:1"~"reboot:2"--24hour
    FlashWrite_reboot(1);                                                       // 更新Flash，准备复位
    liot_trace("------ResetECSystemReset()------");
    liot_rtos_task_sleep_s(1);                                                  // 运行时信息不全，这里加入延时
    ResetECSystemReset();
    while(1);   
}


// UART 触发数据上报处理函数
void handle_uart_trigger(comm_msg_t *msg) {
    if(msg->type == MSG_TYPE_UART_TRIGGER_SWITCH_STATUS_CHANGE) {
        liot_trace("[platform] enMSGID_UART_IO_CHANGE received, sending data to platform...");
        if(datasend_n_times_with_reconnect(en_CMD_UART_SWITCH_IO, 2)){
            cat1.heart_cnt = 0;                                                     // 数据发送成功，心跳包对应延迟
        }
    }
    
    if(msg->type == MSG_TYPE_UART_TRIGGER_SWITCH_PARAM_UPDATE) {
        liot_trace("[platform] enMSGID_UART_PARAM_UPDATE received, sending data to platform...");
        if(datasend_n_times_with_reconnect(en_CMD_UART_SWITCH_PARAM_UPDATE, 2)){
            cat1.heart_cnt = 0;                                                     // 数据发送成功，心跳包对应延迟
        }
    }

    if(msg->type == MSG_TYPE_UART_TRIGGER_MOTOR_STATUS_CHANGE) {
        liot_trace("[platform] enMSGID_UART_MOTOR_CHANGE received, sending data to platform...");
        if(datasend_n_times_with_reconnect(en_CMD_UART_MOTOR_IO, 2)){
            cat1.heart_cnt = 0;                                                     // 数据发送成功，心跳包对应延迟
        }
    }
    
    if(msg->type == MSG_TYPE_UART_TRIGGER_MOTOR_PARAM_UPDATE) {
        liot_trace("[platform] enMSGID_UART_MOTOR_PARAM_UPDATE received, sending data to platform...");
        if(datasend_n_times_with_reconnect(en_CMD_UART_MOTOR_PARAM_UPDATE, 2)){
            cat1.heart_cnt = 0;                                                     // 数据发送成功，心跳包对应延迟
        }
    }
}



/*******************************************************************************
*名    称：handle_platform_trigger()
*功    能：处理平台下行消息的触发事件
*入口参数：msg - 指向 comm_msg_t 类型的指针，包含平台下行消息的相关信息
*说    明：处理平台下行消息逻辑
*示    例：
    comm_msg_t msg;
    handle_platform_trigger(&msg);
*修    改：
*******************************************************************************/
void handle_platform_trigger(comm_msg_t *msg) {
    if(msg->type == MSG_TYPE_PLATFORM_TRIGGER_DOWNLINK) {
        liot_trace("Platform downlink trigger received, reading data...");
        SeverDataRead();
    }
}


/******************************************************************************
*名    称：SeverDataRead
*功    能：服务器下行数据读取
*说    明：
*******************************************************************************/
void SeverDataRead(){
    //Step1: 确认是否有待读取的数据
    if(!cat1.rcv.flag) return;
    cat1.rcv.flag = 0;
    cat1.server_downlink_flag = 1;                                              // 标志位，表示有平台下行数据，心跳包中判断并更新心跳数据，标识topic是否订阅成功
    liot_trace("    1. rcv len: %d, rcv data: %s", cat1.rcv.len, cat1.rcv.buf);

    //Step2: 提取下发的命令(这里不能提取data，因为有"定时Table"会溢出)
    memset(cat1.rcv.cmd, 0, sizeof(cat1.rcv.cmd));                              // 获取之前，先清0

    #if CT_AEP
    extractJson(cat1.rcv.buf, "cmd", cat1.rcv.cmd, sizeof(cat1.rcv.cmd));       // 提取命令到"key"中
    #elif CMCC_ONENET
    cat1.rcv.onenet_id = parse_onenet_id(cat1.rcv.buf);
    liot_trace("    2. extract onenet id: %d", cat1.rcv.onenet_id);
    parse_onenet_params(cat1.rcv.buf, cat1.rcv.onenet_buf);
    liot_trace("    2. extract onenet data: %s", cat1.rcv.onenet_buf);
    extractJson(cat1.rcv.onenet_buf, "cmd", cat1.rcv.cmd, sizeof(cat1.rcv.cmd));// 提取命令到"key"中
    #endif
    liot_trace("    2. extract cmd: %s", cat1.rcv.cmd);

    //Step3: 执行对应命令
    SeverDataRead_CmdExcute(cat1.rcv.cmd);                                      //执行对应的命令
}


/*******************************************************************************
*名    称：SeverDataRead_CmdExcute()
*功    能：执行服务器下发的命令位
*入口参数：serCmd 服务器下发的命令
*说    明：
*修    改：25-07-06 任意平台下行数据，心跳包进行3min延迟
           25-11-26 删除正反转不支持的"MemState"、"TimeTabl"、"TimeStop"命令
           26-02-24 增加ServerDataRead_CmdExcute_PlatformAck()，针对OneNet平台下发的命令，先回复ACK，再执行对应命令
*******************************************************************************/
void SeverDataRead_CmdExcute(char *serCmd){
    if (strcmp(serCmd, "RmtSwich") == 0) {
        uart2.cmd4_platform_rcv_cnt = 0;        // 远程开关命令收到，清除保持计数，解决 UART 心跳包导致状态变更问题
        uart2.cmd7_platform_rcv_cnt = 0;        // 远程电机控制命令收到，清除保持计数，解决 UART 心跳包导致状态变更问题
        SeverDataRead_CmdExcute_RmtSwich();
    }

    else if(strcmp(serCmd, "LinkTest") == 0){
        SeverDataRead_CmdExcute_LinkTest();
    }

    else if(strcmp(serCmd, "MemState") == 0){
        SeverDataRead_CmdExcute_MemState();
    }

    else if (strcmp(serCmd, "DiffFota") == 0){
        SeverDataRead_CmdExcute_Fota_Ack();
        // 已确认不需要额外反馈（内部函数已拼接），只有异常的时候才需要先反馈平台
        // #if CMCC_ONENET
        // ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_SUC);     // 先反馈OneNet平台
        // #endif
        liot_create_fota_task();
    }

    else if (strcmp(serCmd, "ParamGet") == 0){
        SeverDataRead_ParamGet();
    }

    // else if (strcmp(serCmd, "SetLimit") == 0){      // Only AEP 支持电压、电流，OneNet暂不加入 25-10-05
    //     SeverDataRead_SetLimit();
    // }

    else if (strcmp(serCmd, "ModlInfo") == 0){       // onenet平台需要先回复ACK，再反馈信息，分两步执行 26-02-23
        SeverDataRead_ModlInfo();
    }

    else{
        #if CMCC_ONENET
        ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_FAIL);     // 先反馈OneNet平台
        #endif
        iot_datasend_fault(en_FAULT_CMD);
    }
    cat1.heart_cnt = 0;                             // 平台下行交互，心跳包对应延迟
}


/*******************************************************************************
*名    称：SeverDataRead_CmdExcute_Extract_msgID
*功    能：提取服务器下发命令时对应的msgID，存储到cat1.rcv.SerRandomID
*说    明：因为多个函数共用，所以单独提取
*修    改：
*******************************************************************************/
void SeverDataRead_CmdExcute_Extract_msgID(){
    //Step3：提取服务器下发的msgID，异常(格式错误)or正常(定时到达)时返回对应的msgID信息
    memset(cat1.rcv.SerRandomID, 0, sizeof(cat1.rcv.SerRandomID));
    #if CT_AEP
    extractJson(cat1.rcv.buf, "msgID", cat1.rcv.SerRandomID, sizeof(cat1.rcv.SerRandomID));    // 反馈 ACK 时使用
    #elif CMCC_ONENET
    extractJson(cat1.rcv.onenet_buf, "msgID", cat1.rcv.SerRandomID, sizeof(cat1.rcv.SerRandomID));    // 反馈 ACK 时使用
    #endif
    liot_trace("    3. cat1.rcv.SerRandomID: %s", cat1.rcv.SerRandomID);
}


/*******************************************************************************
*名    称：SeverDataRead_CmdExcute_Extract_msgID_and_data
*功    能：提取服务器下发命令时对应的msgID和data，存储到cat1.rcv.SerRandomID和cat1.rcv.data中
*说    明：因为多个函数共用，所以单独提取
*修    改：
*******************************************************************************/
void SeverDataRead_CmdExcute_Extract_msgID_and_data(){
    //Step3：提取服务器下发的msgID，异常(格式错误)or正常(定时到达)时返回对应的msgID信息
    memset(cat1.rcv.SerRandomID, 0, sizeof(cat1.rcv.SerRandomID));
    memset(cat1.rcv.data, 0, sizeof(cat1.rcv.data));                                // 获取之前，先清0
    #if CT_AEP
    extractJson(cat1.rcv.buf, "msgID", cat1.rcv.SerRandomID, sizeof(cat1.rcv.SerRandomID));    // 反馈 ACK 时使用
    extractJson(cat1.rcv.buf, "data", cat1.rcv.data, sizeof(cat1.rcv.data));        // 提取命令到"value"中
    #elif CMCC_ONENET
    extractJson(cat1.rcv.onenet_buf, "msgID", cat1.rcv.SerRandomID, sizeof(cat1.rcv.SerRandomID));    // 反馈 ACK 时使用
    extractJson(cat1.rcv.onenet_buf, "data", cat1.rcv.data, sizeof(cat1.rcv.data));            // 提取命令到"value"中
    #endif
    liot_trace("    3. cat1.rcv.SerRandomID: %s, data: %s", cat1.rcv.SerRandomID, cat1.rcv.data);
}

void datasend_fillproto_common_replace_output(e_COMMON_UP_COMMAND com){

#if PRODUCT_SWITCH
    char out_state[5] = "0000";  
    out_state[4] = 0;                               // 字符串末尾\0

    // 根据当前输出状态替换字符串，默认是"0000"，最多支持4路输出
    for (int i = 0; i < 4; i++) {
        if (g_io.out[i].now_status == enIO_OUT_ON) {
            out_state[i] = '1';
        } else {
            out_state[i] = '0';
        }
    }

    #if CT_AEP                              // AEP平台
    if(com == en_CMD_POWERON){
        replaceJson(aep_data_send_poweron, "output", out_state);
    }
    else {
        replaceJson(aep_data_send_all, "output", out_state);    
    }
    #elif CMCC_ONENET                       // OneNet平台
    if(com == en_CMD_POWERON){
        replaceJson(onenet_data_poweron, "output", out_state);
    }
    else {
        replaceJson(onenet_data_common, "output", out_state);
    }
    #endif

#elif PRODUCT_MOTOR
    const char *out_state = NULL;
    const char *workmode = NULL;
    
    switch(motor.currentState){
        case MODE_IDLE_HOLD:
        case MODE_IDLE_JOG:
            out_state = "stop";
            break;
        case MODE_FORWARD_HOLD:
        case MODE_FORWARD_JOG:
            out_state = "forw";
            break;
        case MODE_REVERSE_HOLD:
        case MODE_REVERSE_JOG:
            out_state = "revr";
            break;
        default:
            out_state = "stop";
            break;
    }

    /* 根据电机状态确定 workmode */
    if (motor.currentState == MODE_IDLE_JOG || 
        motor.currentState == MODE_FORWARD_JOG || 
        motor.currentState == MODE_REVERSE_JOG) {
        workmode = "Jog";  // 点动模式
    } else {
        workmode = "Hld";  // 联动模式
    }
    // const char *workmode = (MODE_IDLE_JOG == motor.currentState) ? "Jog" : "Hld"; 26-03-14 发现bug
    
    #if CT_AEP                              // AEP平台
    if(com == en_CMD_POWERON){
        replaceJson(aep_data_send_poweron, "output", out_state);
        replaceJson(aep_data_send_poweron, "workmode", workmode);
    }
    else {
        replaceJson(aep_data_send_all, "output", out_state);
        replaceJson(aep_data_send_all, "workmode", workmode);
    }
    #elif CMCC_ONENET                       // OneNet平台
    
    #endif

#endif
}

/*******************************************************************************
 *  名    称：SeverDataRead_CmdExcute_MemState
 * 功    能：状态记忆命令执行函数，根据不同的存储位置执行对应的函数
 * 入口参数：
 * 说    明：针对OneNet平台下发的命令，先回复ACK，再执行对应命令
 * 修    改：
 * *******************************************************************************/
void SeverDataRead_CmdExcute_MemState() {
    
    #if MEM_STATE_CAT1_FLASH                // 状态记忆存储到Cat.1
    liot_trace("MemState_Cat1");
    SeverDataRead_CmdExcute_MemState_Cat1();


    #elif MEM_STATE_MCU_FLASH               // 状态记忆存储到MCU，Cat.1只是中转，执行对应命令后会通过UART发送给MCU，MCU执行后再返回结果，Cat.1根据结果反馈平台
    liot_trace("MemState_Mcu");
    SeverDataRead_CmdExcute_MemState_Mcu();

    #endif
}

/*******************************************************************************
*名    称：get_platform_cmd_confirm_result_str()
*功    能：将e_PLATFORM_CMD_CONFIRM_RESULT枚举值转换为字符串
*说    明：用于日志打印，使枚举值更直观
*修    改：
*******************************************************************************/  
const char* get_platform_cmd_confirm_result_str(e_PLATFORM_CMD_CONFIRM_RESULT result) {
    switch (result) {
        case en_PLATFORM_CMD_CONFIRM_SUC:
            return "en_PLATFORM_CMD_CONFIRM_SUC (0)";
        case en_PLATFORM_CMD_CONFIRM_FAIL:
            return "en_PLATFORM_CMD_CONFIRM_FAIL (1)";
        case en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO:
            return "en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO (2)";
        case en_PLATFORM_CMD_CONFIRM_FAIL_MEM_STATE:
            return "en_PLATFORM_CMD_CONFIRM_FAIL_MEM_STATE (3)";
        case en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK:
            return "en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK (4)";
        default:
            return "Unknown (invalid value)";
    }
}

/*******************************************************************************
 * 名    称：SeverDataRead_CmdExcute_MemState_Mcu
 * 功    能：状态记忆命令执行函数，状态记忆存储到MCU，Cat.1只是中转，执行对应命令后会通过UART发送给MCU，MCU执行后再返回结果，Cat.1根据结果反馈平台
 * 入口参数：
 * 说    明：针对OneNet平台下发的命令，先回复ACK，再执行对应命令
 * 修    改：
 * *******************************************************************************/
void SeverDataRead_CmdExcute_MemState_Mcu() {
    liot_trace("Extract msgID and data for MemState");
    SeverDataRead_CmdExcute_Extract_msgID_and_data();                       // 提取msgID和data

    e_PLATFORM_CMD_CONFIRM_RESULT confirm_result = MemState_ConfirmUART();  // 获取UART切换结果
    liot_trace("MemState_ConfirmUART() result: %s", get_platform_cmd_confirm_result_str(confirm_result));
    
    MemState_Feedback(confirm_result);
}

/*******************************************************************************
*名    称：MemState_ConfirmUART
*功    能：
*入口参数：
*出口参数：
*******************************************************************************/
e_PLATFORM_CMD_CONFIRM_RESULT MemState_ConfirmUART() {
    // Step1: 根据服务器下发的data，判断需要切换的状态，并更新cat1.memState，供后续发送给UART
    if (strcmp(cat1.rcv.data, "Open") == 0) {
        strcpy(cat1.memState, "1111");
    }
    else if (strcmp(cat1.rcv.data, "Clos") == 0) {
        strcpy(cat1.memState, "0000");
    }
    else if (strcmp(cat1.rcv.data, "Hold") == 0) {          
        strcpy(cat1.memState, "PREV");
    }
    else{
        liot_trace("Error: Unrecognized data for MemState: %s", cat1.rcv.data);
        return en_PLATFORM_CMD_CONFIRM_FAIL_MEM_STATE; // 数据异常，返回执行失败
    }

    // Step2: 发送 UART 状态记忆命令，等待反馈
    uart2.isMemStateFeedback = 0;                       // 收到平台下发的状态记忆命令，等待 UART 反馈 ACK
    
    // 直接发送 UART 消息，不再通过队列
    for (int i = 0; i < 3; i++) {
        liot_trace("[UART2] drv_send_cmd9_push_%s %d times", 
                   UART_PROTOCOL_FORMAT_ASCII ? "str" : "hex", i+1);
        
        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd9_push_str("MEM");                  // ASCII 协议：发送 CMD9 推送帧
        #elif UART_PROTOCOL_FORMAT_HEX
        // 根据 cat1.memState 计算 Hex 协议的 value 值，并保存到 uart2.lastSentMemValue
        uart2.lastSentMemValue = 0x00;   // 默认：上电关
        if (strcmp(cat1.memState, "0000") == 0) {
            uart2.lastSentMemValue = 0x00;       // 上电关
        }
        else if (strcmp(cat1.memState, "1111") == 0) {
            uart2.lastSentMemValue = 0x01;       // 上电开
        }
        else if (strcmp(cat1.memState, "PREV") == 0) {
            uart2.lastSentMemValue = 0x02;       // 上电保持
        }
        drv_send_cmd9_push_hex();              // Hex 协议：发送 CMD=0x09 推送帧
        #endif
        
        // 等待 UART 反馈
        for (int j = 0; j < 10; j++) {
            liot_rtos_task_sleep_ms(50);                // 每次等待 50ms
            if (uart2.isMemStateFeedback) {
                liot_trace("-------> return en_PLATFORM_CMD_CONFIRM_SUC <-------");
                return en_PLATFORM_CMD_CONFIRM_SUC;
            }
        }
        liot_trace("UART2 MEM state feedback wait timeout, retrying...");
    }
    
    // 如果多次发送仍未收到反馈，返回失败
    uart2.faultType = UART_FAULT_NO_FEEDBACK;
    liot_trace("UART2 MEM state feedback not received after retries");
    return en_PLATFORM_CMD_CONFIRM_FAIL_MEM_STATE;

    // Step3: UART未使能，直接返回成功 —— 这里不存在UART未使能的情况，因为这个函数就是UART状态记忆命令的执行函数
    // return en_PLATFORM_CMD_CONFIRM_SUC;
}

/*******************************************************************************
*名    称：MemState_Feedback
*功    能：根据确认结果，反馈平台
*入口参数：
*说    明：
*修    改：
*******************************************************************************/
void MemState_Feedback(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result) {
    switch (confirm_result) {
        case en_PLATFORM_CMD_CONFIRM_SUC:
            if (strcmp(cat1.rcv.data, "Open") == 0) {
                iot_datasend_ack(en_CMD_POWERON_STATUS_OPEN);                   // Send ACK
            }
            else if (strcmp(cat1.rcv.data, "Clos") == 0) {
                iot_datasend_ack(en_CMD_POWERON_STATUS_CLOS);                   // Send ACK
            }
            else if (strcmp(cat1.rcv.data, "Hold") == 0) {          
                iot_datasend_ack(en_CMD_POWERON_STATUS_HOLD);                   // Send ACK
            }
            // 移动平台无需额外反馈，协议拼接前缀已经附带onenet_payload_ack
            // #if CMCC_ONENET
            // ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_SUC); // Feedback to OneNet platform
            // #endif
            break;

        
        case en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK:
            #if CMCC_ONENET
            ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_FAIL); // Feedback to OneNet platform
            #endif
            iot_datasend_fault(en_FAULT_UART_NO_FEEDBACK);  // Send UART feedback fault
            break;

        default:
            #if CMCC_ONENET
            ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_FAIL); // Feedback to OneNet platform
            #endif
            iot_datasend_fault(en_FAULT_UNKNOWN);           // Send unknown fault
            break;
    }
}


/*******************************************************************************
*名    称：SeverDataRead_CmdExcute_MemState()
*功    能：执行服务器下发的"状态记忆"命令
*下发示例：{"cmd":"MemState","data":"Open","msgID":"36E2D5","workmode":"Swt"}
*修    改：
*******************************************************************************/
void SeverDataRead_CmdExcute_MemState_Cat1(){
    SeverDataRead_CmdExcute_Extract_msgID_and_data();           // 提取msgID和data

    if (strcmp(cat1.rcv.data, "Open") == 0) {
        FlashWrite_PowerOnStatus(enSTATUS_PWR_OUT_H); 
        if(!FlashRead_CheckModeSuc(enSTATUS_PWR_OUT_H)){       // 检查1次
            iot_datasend_ack(en_CMD_FLASH_ERROR_WRITE);
            FlashWrite_PowerOnStatus(enSTATUS_PWR_OUT_H); 
            if(!FlashRead_CheckModeSuc(enSTATUS_PWR_OUT_H)){   // 检查2次
                iot_datasend_ack(en_CMD_FLASH_ERROR_WRITE);
                return;
            }
        }
        iot_datasend_ack(en_CMD_POWERON_STATUS_OPEN);
    }

    else if (strcmp(cat1.rcv.data, "Clos") == 0) {
        FlashWrite_PowerOnStatus(enSTATUS_PWR_OUT_L);  
        if(!FlashRead_CheckModeSuc(enSTATUS_PWR_OUT_L)){       // 检查1次
            iot_datasend_ack(en_CMD_FLASH_ERROR_WRITE);
            FlashWrite_PowerOnStatus(enSTATUS_PWR_OUT_L); 
            if(!FlashRead_CheckModeSuc(enSTATUS_PWR_OUT_L)){   // 检查2次
                iot_datasend_ack(en_CMD_FLASH_ERROR_WRITE);
                return;
            }
        }
        iot_datasend_ack(en_CMD_POWERON_STATUS_CLOS);
    }
    
    else if (strcmp(cat1.rcv.data, "Hold") == 0) {          
        flash_poweron.mode[0] = 'p';                        // 这里需要赋值，否则重启后才能执行，位置必须放在最前面 24-11-19
        flash_poweron.mode[1] = 'r';
        flash_poweron.mode[2] = 'e';
        flash_poweron.mode[3] = 0;
        FlashWrite_PowerOnStatus(enSTATUS_PWR_OUT_HOLD); 
        if(!FlashRead_CheckModeSuc(enSTATUS_PWR_OUT_HOLD)){    // 检查1次
            iot_datasend_ack(en_CMD_FLASH_ERROR_WRITE);
            FlashWrite_PowerOnStatus(enSTATUS_PWR_OUT_HOLD); 
            if(!FlashRead_CheckModeSuc(enSTATUS_PWR_OUT_HOLD)){// 检查2次
                iot_datasend_ack(en_CMD_FLASH_ERROR_WRITE);
                return;
            }
        }
        iot_datasend_ack(en_CMD_POWERON_STATUS_HOLD);
    }
    else{
        liot_trace(" cmd \"MemState\" UNknown data, cannot excute!");
        iot_datasend_ack(en_CMD_POWERON_STATUS_FAIL);
    }
}



/*******************************************************************************
*名    称：SeverDataRead_CmdExcute_RmtSwich()
*功    能：执行服务器下发的"远程开/关"命令（Switch 产品）或"LocSwich"命令（Motor 产品）
*下行示例：
*   Switch: {"cmd":"RmtSwich","data":"0001","msgID":"36E2D5","workmode":"Swt"}
*   Motor:  {"cmd":"RmtSwich","data":"Forw","msgID":"36E2D5","workmode":"Jog"}
*修    改：26-03-15 根据产品类型分别处理 Switch 和 Motor 逻辑
*******************************************************************************/
void SeverDataRead_CmdExcute_RmtSwich() {
    SeverDataRead_CmdExcute_Extract_msgID_and_data();                           // 提取msgID和data

#if PRODUCT_SWITCH
    e_PLATFORM_CMD_CONFIRM_RESULT confirm_result = RmtSwich_ConfirmIOState();   // 获取IO状态切换的结果
    RmtSwich_ExcuteIo_and_UpdateIoState(confirm_result);                        // 根据IO状态切换的结果，执行对应的操作，这里要放到UART之后，确保状态切换成功才执行对应的操作
    RmtSwich_Feedback_to_Platform(confirm_result);                              // 根据IO状态切换的结果，执行对应的操作并反馈平台
#elif PRODUCT_MOTOR
    memset(cat1.rcv.workmode, 0, sizeof(cat1.rcv.workmode));
    extractJson(cat1.rcv.buf, "workmode", cat1.rcv.workmode, sizeof(cat1.rcv.workmode));
    liot_trace("    3. cat1.rcv.workmode: %s", cat1.rcv.workmode);

    e_PLATFORM_CMD_CONFIRM_RESULT confirm_result = LocSwich_ConfirmMotorState();// 获取电机状态切换的结果
    LocSwich_Excute_and_UpdateMotorState(confirm_result);                                  // 根据电机状态切换的结果，执行对应的操作，这里要放到UART之后，确保状态切换成功才执行对应的操作
    RmtSwich_Feedback_to_Platform(confirm_result);                        // 根据电机状态切换的结果，执行对应的操作并反馈平台
#endif
}

/*******************************************************************************
*名    称：RmtSwich_ConfirmIOState()
*功    能：确认平台下发的IO状态是否合法，如果UART使能，等待UART反馈切换结果
*入口参数：无
*出口参数：返回确认结果
*******************************************************************************/
e_PLATFORM_CMD_CONFIRM_RESULT RmtSwich_ConfirmIOState() {
    // Step1: 确认平台下发的IO状态是否合法，并更新到g_io.out.now_status中
    for (int i = 0; i < 4; i++) {
        liot_trace("cat1.rcv.data[%d]: %c", i, cat1.rcv.data[i]);
        if (cat1.rcv.data[i] == '0') {
            g_io.out[i].now_status = enIO_OUT_OFF;          // 当前状态设置为关机
        } else if (cat1.rcv.data[i] == '1') {
            g_io.out[i].now_status = enIO_OUT_ON;           // 当前状态设置为开机
        } else {
            return en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO;  // 数据异常，返回执行失败
        }
    }

    // Step2: 如果UART使能，等待UART反馈切换结果
    #if UART_COMM_ENABLE
    uart2.isIoSwitchFeedback = 0;                       // 收到平台下发的IO切换命令，等待UART反馈ACK

    // 直接发送 UART 消息，不再通过队列
    for (int i = 0; i < 3; i++) {
        liot_trace("[UART2] drv_send_cmd4_push_%s %d times", 
                   UART_PROTOCOL_FORMAT_ASCII ? "str" : "hex", i+1);
        
        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd4_push_str();                       // ASCII 协议：发送 CMD4 推送帧
        #elif UART_PROTOCOL_FORMAT_HEX
        drv_send_cmd4_push_hex();                       // Hex 协议：发送 CMD4 推送帧
        #endif
        
        // 等待 UART 反馈
        for (int j = 0; j < 50; j++) {
            liot_rtos_task_sleep_ms(10);                // 每次等待 10ms
            if (uart2.isIoSwitchFeedback) {
                liot_trace("-------> UART2 IO switch feedback received <-------");
                return en_PLATFORM_CMD_CONFIRM_SUC;
            }
        }
        liot_trace("UART2 IO switch feedback wait timeout, retrying...");
    }
    
    // 如果多次发送仍未收到反馈，返回失败
    uart2.faultType = UART_FAULT_NO_FEEDBACK;
    liot_trace("UART2 IO switch feedback not received after retries");
    return en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK;  // 未收到UART反馈，返回执行失败
    #endif

    // Step3: UART未使能，直接返回成功
    return en_PLATFORM_CMD_CONFIRM_SUC;
}

/*******************************************************************************
 * 名    称：RmtSwich_ExcuteIo_and_UpdateIoState
 * 功    能：根据平台下发的IO状态切换结果，执行对应的操作
 * 入口参数：confirm_result 平台下发IO状态的确认结果
 * 说    明：如果切换成功，执行IO切换操作；如果切换失败，保持原有状态不变
 * 注意事项：这里要放到UART之后，如果开启了UART，必须确保状态切换成功才执行对应的操作
 * 修    改：26-03-08 如果开启了UART且没有收到正确的反馈，则不在执行本地IO状态变化
 * *******************************************************************************/
void RmtSwich_ExcuteIo_and_UpdateIoState(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result){
    // Handle IO state updates based on confirmation result
    if (confirm_result == en_PLATFORM_CMD_CONFIRM_SUC) {
        handle_successful_confirmation();
    } else {
        handle_failed_confirmation();
    }
}


/*******************************************************************************
*名    称：RmtSwich_Feedback_to_Platform
*功    能：根据确认结果，执行IO切换操作并反馈平台
*入口参数：confirm_result 平台下发IO状态的确认结果
*说    明：
*修    改：
*******************************************************************************/
void RmtSwich_Feedback_to_Platform(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result) {
    // Send feedback based on confirmation result
    switch (confirm_result) {
        case en_PLATFORM_CMD_CONFIRM_SUC:
            iot_datasend_ack(en_CMD_REMOTE_SWITCH);         // Send ACK
            break;

        case en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO:
            iot_datasend_fault(en_FAULT_REMOTE_IO);         // Send remote IO fault
            break;

        case en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK:
            #if CMCC_ONENET
            ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_FAIL);    // Feedback to OneNet platform
            #endif
            iot_datasend_fault(en_FAULT_UART_NO_FEEDBACK);  // Send UART feedback fault
            break;

        default:
            iot_datasend_fault(en_FAULT_UNKNOWN);           // Send unknown fault
            break;
    }
}

/*******************************************************************************
 * 以下函数针对 Motor 产品，处理电机控制命令
 ******************************************************************************/

/*******************************************************************************
 * 名    称：LocSwich_ConfirmMotorState
 * 功    能：确认平台下发的电机状态是否合法，如果 UART 使能，等待 UART 反馈切换结果
 * 入口参数：无
 * 出口参数：返回确认结果
 * 说    明：
 *  - 解析平台下发的 workmode 和 data 字段
 *  - 使用 strcmp 判断 workmode 和 data 的具体内容
 *  - 根据 workmode 和 data 的组合确定对应的 motor_state_t
 *  - 如果 UART 使能，等待 UART 反馈
 * 下行示例：{"cmd":"RmtSwich","data":"Forw","msgID":"36E2D5","workmode":"Jog"}
 * *******************************************************************************/
e_PLATFORM_CMD_CONFIRM_RESULT LocSwich_ConfirmMotorState() {
    liot_trace("[MOTOR] LocSwich_ConfirmMotorState: workmode=%s, data=%s", cat1.rcv.workmode, cat1.rcv.data);
    
    // Step1: 验证工作模式并设置电机状态
    if (strcmp(cat1.rcv.workmode, "Jog") == 0) {
        // 点动模式
        if (strcmp(cat1.rcv.data, "forw") == 0) {
            motor.currentState = MODE_FORWARD_JOG;  // 点动正转
            liot_trace("[MOTOR] Mode: Jog, State: Forw -> MODE_FORWARD_JOG");
        }
        else if (strcmp(cat1.rcv.data, "revr") == 0) {
            motor.currentState = MODE_REVERSE_JOG;  // 点动反转
            liot_trace("[MOTOR] Mode: Jog, State: Revr -> MODE_REVERSE_JOG");
        }
        else if (strcmp(cat1.rcv.data, "stop") == 0) {
            motor.currentState = MODE_IDLE_JOG;     // 点动停止
            liot_trace("[MOTOR] Mode: Jog, State: stop -> MODE_IDLE_JOG");
        }
        else {
            liot_trace("[MOTOR] Invalid data in Jog mode: %s", cat1.rcv.data);
            return en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO;
        }
    }
    else if (strcmp(cat1.rcv.workmode, "Hld") == 0) {
        // 联动模式
        if (strcmp(cat1.rcv.data, "forw") == 0) {
            motor.currentState = MODE_FORWARD_HOLD;  // 联动正转
            liot_trace("[MOTOR] Mode: Hld, State: Forw -> MODE_FORWARD_HOLD");
        }
        else if (strcmp(cat1.rcv.data, "revr") == 0) {
            motor.currentState = MODE_REVERSE_HOLD;  // 联动反转
            liot_trace("[MOTOR] Mode: Hld, State: Revr -> MODE_REVERSE_HOLD");
        }
        else if (strcmp(cat1.rcv.data, "stop") == 0) {
            motor.currentState = MODE_IDLE_HOLD;     // 联动停止
            liot_trace("[MOTOR] Mode: Hld, State: stop -> MODE_IDLE_HOLD");
        }
        else {
            liot_trace("[MOTOR] Invalid data in Hld mode: %s", cat1.rcv.data);
            return en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO;
        }
    }
    else {
        liot_trace("[MOTOR] Invalid workmode: %s", cat1.rcv.workmode);
        return en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO;
    }
    
    // Step2: 如果 UART 使能，等待 UART 反馈
    #if UART_COMM_ENABLE
    uart2.isIoSwitchFeedback = 0;                       // 收到平台下发的电机控制命令，等待 UART 反馈 ACK
    
    // 直接发送 UART 消息
    for (int i = 0; i < 3; i++) {
        liot_trace("[UART2] drv_send_cmd7_push_%s %d times", 
                   UART_PROTOCOL_FORMAT_ASCII ? "str" : "hex", i+1);
        
        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd7_push_str();                       // ASCII 协议：发送 CMD7 推送帧
        #elif UART_PROTOCOL_FORMAT_HEX
        drv_send_cmd7_push_hex();                       // Hex 协议：发送 CMD=0x01 推送帧
        #endif
        
        // 等待 UART 反馈
        for (int j = 0; j < 50; j++) {
            liot_rtos_task_sleep_ms(10);                // 每次等待 10ms
            if (uart2.isIoSwitchFeedback) {
                liot_trace("-------> UART2 Motor control feedback received <-------");
                return en_PLATFORM_CMD_CONFIRM_SUC;
            }
        }
        liot_trace("UART2 Motor control feedback wait timeout, retrying...");
    }
    
    // 如果多次发送仍未收到反馈，返回失败
    uart2.faultType = UART_FAULT_NO_FEEDBACK;
    liot_trace("UART2 Motor control feedback not received after retries");
    return en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK;  // 未收到 UART 反馈，返回执行失败
    #endif
    
    // Step3: UART 未使能，直接返回成功
    return en_PLATFORM_CMD_CONFIRM_SUC;
}

/*******************************************************************************
 * 名    称：LocSwich_Excute_and_UpdateMotorState
 * 功    能：根据平台下发的电机状态切换结果，执行对应的操作
 * 入口参数：confirm_result 平台下发状态的确认结果
 * 说    明：如果切换成功，执行电机状态切换操作；如果切换失败，保持原有状态不变
 * 注意事项：这里要放到 UART 之后，如果开启了 UART，必须确保状态切换成功才执行对应的操作
 * *******************************************************************************/
void LocSwich_Excute_and_UpdateMotorState(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result){
    // Handle IO state updates based on confirmation result
    if (confirm_result == en_PLATFORM_CMD_CONFIRM_SUC) {
        liot_trace("[MOTOR] LocSwich_Excute_and_UpdateMotorState: SUCCESS");
        update_motor_io_state();
    } else {
        // 这里不在恢复电机原来的状态，因为小程序已经离线，等待5S内的心跳再次更新即可
    }    
}

/*******************************************************************************
 * 名    称：LocSwich_UpdateMotorState_and_Feedback
 * 功    能：根据确认结果，执行电机状态切换操作并反馈平台
 * 入口参数：confirm_result 平台下发状态的确认结果
 * 说    明：
 * *******************************************************************************/
void LocSwich_UpdateMotorState_and_Feedback(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result) {
    // Handle motor state updates based on confirmation result
    if (confirm_result == en_PLATFORM_CMD_CONFIRM_SUC) {
        liot_trace("[MOTOR] LocSwich_UpdateMotorState_and_Feedback: SUCCESS");
        // 可以在这里添加成功后的额外处理
    } else {
        liot_trace("[MOTOR] LocSwich_UpdateMotorState_and_Feedback: FAILED, result=%d", confirm_result);
        // 可以在这里添加失败后的额外处理
    }
    
    // Send feedback based on confirmation result
    switch (confirm_result) {
        case en_PLATFORM_CMD_CONFIRM_SUC:
            iot_datasend_ack(en_CMD_REMOTE_SWITCH);         // Send ACK
            break;
            
        case en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO:
            iot_datasend_fault(en_FAULT_REMOTE_IO);      // Send remote IO fault
            break;
            
        case en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK:
            #if CMCC_ONENET
            ServerDataRead_CmdExcute_PlatformAck(en_CMD_PLATFORM_FEEDBACK_FAIL);  // Feedback to OneNet platform
            #endif
            iot_datasend_fault(en_FAULT_UART_NO_FEEDBACK);  // Send UART feedback fault
            break;
            
        default:
            iot_datasend_fault(en_FAULT_UNKNOWN);        // Send unknown fault
            break;
    }
}

// Helper function to handle successful confirmation
void handle_successful_confirmation() {
    for (int i = 0; i < 4; i++) {
        g_io.out[i].pre_status = g_io.out[i].now_status; // Update previous status to current
        update_io_state_from_g_io();

        #if MEM_STATE_CAT1_FLASH
        FlashWrite_IOSwitch(); // Write to Flash
        #endif
    }
}

// Helper function to handle failed confirmation
void handle_failed_confirmation() {
    for (int i = 0; i < 4; i++) {
        g_io.out[i].now_status = g_io.out[i].pre_status; // Revert to previous status
    }
}

/*******************************************************************************
*名    称：SeverDataRead_CmdExcute_Fota()
*功    能：执行Fota升级之前，反馈相关信息
*说    明：
*修    改：
*******************************************************************************/
void SeverDataRead_CmdExcute_Fota_Ack(){
    SeverDataRead_CmdExcute_Extract_msgID();                // 提取msgID

    //Step4：提取服务器下发的url
    memset(cat1_fota.url_address, 0, sizeof(cat1_fota.url_address));                            // 清空url
    #if CT_AEP
    extractJson(cat1.rcv.buf, "data", cat1_fota.url_address, LIOT_FOTA_PACK_NAME_MAX_LEN_150);  // 提取data到url_adress中
    #elif CMCC_ONENET
    extractJson(cat1.rcv.onenet_buf, "data", cat1_fota.url_address, LIOT_FOTA_PACK_NAME_MAX_LEN_150);  // 提取data到url_adress中
    #endif
    liot_trace("    4. url_adress: %s", cat1_fota.url_address);    

    if(strlen(cat1_fota.url_address) < 10){                 // 如果长度过小或者过大，extractJson会返回空
        iot_datasend_ack(en_CMD_FOTA_ERROR_LENTH);          // 返回错误提示
        return;
    }
    if(fota_containsPar(cat1_fota.url_address) == false){   // url地址不包含.par
        iot_datasend_ack(en_CMD_FOTA_ERROR_ADRESS);         // 返回错误提示
        return;
    }

    iot_datasend_ack(en_CMD_FOTA_START);                    // 返回Fota启动提示
    liot_rtos_task_sleep_ms(10);
}

bool fota_containsPar(const char *str) {
    const char *substring = ".par";
    return strstr(str, substring) != NULL;                  // 如果找到，返回 true；否则返回 false
}

void iot_ack_remote_switch_replace_data_workmode(){
#if PRODUCT_SWITCH
    char out_state[5] = "0000";

    out_state[0] = (g_io.out[0].now_status == enIO_OUT_ON) ? '1' : '0';
    out_state[1] = (g_io.out[1].now_status == enIO_OUT_ON) ? '1' : '0';
    out_state[2] = (g_io.out[2].now_status == enIO_OUT_ON) ? '1' : '0';
    out_state[3] = (g_io.out[3].now_status == enIO_OUT_ON) ? '1' : '0';
    out_state[4] = 0;                               // 字符串末尾\0

    replaceJson(iot_data_ack, "data", out_state);

#elif PRODUCT_MOTOR
    replaceJson(iot_data_ack, "workmode", cat1.rcv.workmode);
    replaceJson(iot_data_ack, "data", cat1.rcv.data);
#endif
}

/*******************************************************************************
*名    称：SeverDataRead_CmdExcute_LinkTest()
*功    能：执行服务器下发的"链路测试"命令
*下行示例: {"cmd":"LinkTest","data":"null","msgID":"36E2D5"}
*修    改：
*******************************************************************************/
void SeverDataRead_CmdExcute_LinkTest(){
    SeverDataRead_CmdExcute_Extract_msgID_and_data();       // 提取msgID和data
    iot_datasend_ack(en_CMD_POWERON_LINK_TEST);             // 反馈ACK  
}

/*******************************************************************************
 * 名    称：SeverDataRead_CmdExcute_PlatformAck()
 * 功    能：执行服务器下发的命令后，如果无法匹配到具体的命令，或者命令执行完成后需要反馈ACK时，调用此函数
 * 说    明：提取msgID并反馈ACK，适用于多个命令的ACK反馈
 * 修    改：
 * *******************************************************************************/
void ServerDataRead_CmdExcute_PlatformAck(e_IOT_DOWN_COMMAND cmd){
    SeverDataRead_CmdExcute_Extract_msgID();                // 提取msgID
    iot_datasend_ack(cmd);             // 反馈ACK  
}

/*******************************************************************************
*名    称：iot_datasend_fault()
*功    能：模组无法解析或者遇到异常时发送错误信息给到平台
*说    明：将原aep_datasend_fault封装，合入onenet_datasend_fault
*******************************************************************************/    
void iot_datasend_fault(e_FAULT_TYPE cmd){
    #if CT_AEP
    aep_datasend_fault(cmd);                  
    #elif CMCC_ONENET
    onenet_datasend_fault(cmd);
    #endif
}

/*******************************************************************************
 *  名    称：iot_datasend_ack()
 *  功    能：模组发送数据成功后，发送ACK给到平台
 *  说    明：将原aep_datasend_ack封装，合入onenet_datasend_ack
 * *****************************************************************************/
void iot_datasend_ack(e_IOT_DOWN_COMMAND cmd){
    #if CT_AEP
    aep_datasend_ack(cmd);                  
    #elif CMCC_ONENET
    onenet_datasend_ack(cmd);
    #endif
}


/*******************************************************************************
 * @brief 根据ICCID自动识别运营商
 * @param iccid 输入的ICCID字符串（长度>=6）
 * @return operator_t 返回识别的运营商类型
    00：移动 02：移动 04：移动 07：移动 08：移动 20~29：移动
    01：联通 06：联通 09：联通 10：联通 30~39：联通
    03：电信 11：电信 12：电信 50~59：电信
    05：未知
 * *****************************************************************************/
operator_t get_iccid_operator(const char *iccid)
{
    if (iccid == NULL || strlen(iccid) < 6) {
        liot_trace("get_iccid_operator: invalid iccid, NULL or too short");
        return OP_UNKNOWN;
    }

    // 校验前缀 "8986"
    if (strncmp(iccid, "8986", 4) != 0) {
        liot_trace("get_iccid_operator: iccid prefix not match '8986': %s", iccid);
        return OP_UNKNOWN;
    }

    // 提取第5、第6位号段（例如 "00", "01", "20"...）
    char seg_str[3] = {0};
    memcpy(seg_str, &iccid[4], 2); // 仅取第5、第6位
    int seg = atoi(seg_str);

    liot_trace("get_iccid_operator: extracted seg_str='%s', seg=%d", seg_str, seg);

    // 匹配规则
    if (seg == 0 || seg == 2 || seg == 4 || seg == 7 || seg == 8 || (seg >= 20 && seg <= 29)){
        liot_trace("get_iccid_operator: operator detected CMCC for seg=%d", seg);
        return OP_CMCC;  // 移动
    }
    else if (seg == 1 || seg == 6 || seg == 9 || seg == 10 || (seg >= 30 && seg <= 39)){
        liot_trace("get_iccid_operator: operator detected CUCC for seg=%d", seg);
        return OP_CUCC;  // 联通
    }
    else if (seg == 3 || seg == 11 || seg == 12 || (seg >= 50 && seg <= 59)){
        liot_trace("get_iccid_operator: operator detected CTCC for seg=%d", seg);
        return OP_CTCC;  // 电信
    }
    else{
        liot_trace("get_iccid_operator: operator UNKNOWN for seg=%d", seg);
        return OP_UNKNOWN;  // 其他未知号段
    }
}

/*******************************************************************************
*名    称：ICCID_OperatorCheck()
*功    能：ICCID范围检查，必须符合规定范围内的ICCID才能进行下一步
*说    明：
*修    改：
*******************************************************************************/          
void ICCID_OperatorCheck(){
    #if CT_AEP
    if(get_iccid_operator(cat1.iccid) != OP_CTCC){
        while(1){
            liot_trace("ICCIC %s not match CTCC(AEP)",cat1.iccid);
            cat1_reg_led_check(en_CAT1_LED_SIM_MATCH);             // LED闪烁提示
            liot_rtos_task_sleep_ms(100);

            #if UART_PROTOCOL_FORMAT_ASCII
            drv_send_cmd3_push_net_status();            // 通过UART发送当前注册状态，便于MCU判断
            #endif
        }
    }

    #elif CMCC_ONENET
    if(get_iccid_operator(cat1.iccid) != OP_CMCC){
        while(1){
            liot_trace("ICCIC %s not match CMCC(Onenet)",cat1.iccid);
            cat1_reg_led_check(en_CAT1_LED_SIM_MATCH);             // LED闪烁提示
            liot_rtos_task_sleep_ms(100);
        }
    }

    #endif

    liot_trace("ICCIC match Operator");
}




// Helper: stringify trigger enums for readable logging (shared implementation)
const char *trigger_source_to_str(trigger_source_t s){
    switch(s){
        case TRIGGER_SOURCE_UART: return "UART";
        case TRIGGER_SOURCE_KEY: return "KEY";
        case TRIGGER_SOURCE_TIMER: return "TIMER";
        case TRIGGER_SOURCE_PLATFORM: return "PLATFORM";
        default: return "UNKNOWN";
    }
}


const char *trigger_type_to_str(trigger_type_t t){
    switch(t){
        case MSG_TYPE_TIMER_TRIGGER_HEARTBEAT: return "HEARTBEAT";
        case MSG_TYPE_TIMER_TRIGGER_RECONNECT: return "RECONNECT";
        case MSG_TYPE_UART_TRIGGER_SWITCH_STATUS_CHANGE: return "IO_STATUS_CHANGE";
        case MSG_TYPE_UART_TRIGGER_SWITCH_PARAM_UPDATE: return "PARAM_UPDATE";
        case MSG_TYPE_UART_TRIGGER_MOTOR_STATUS_CHANGE: return "MOTOR_STATUS_CHANGE";
        case MSG_TYPE_UART_TRIGGER_MOTOR_PARAM_UPDATE: return "MOTOR_PARAM_UPDATE";
        case MSG_TYPE_PLATFORM_TRIGGER_DOWNLINK: return "DOWNLINK";
        case MSG_TYPE_UART_RCV_TRIGGER: return "UART_RCV";
        case MSG_TYPE_KEY_TRIGGER_IO_STATUS_CHANGE: return "KEY_IO_CHANGE";
        default: return "UNKNOWN";
    }
}

/***************************************************************************
 * 名    称：log_received_message
 * 功    能：记录接收到的消息信息
 * 参    数：msg - 指向 comm_msg_t 结构体的指针
 ***************************************************************************/
void log_received_message(const comm_msg_t *msg) {
    if (!msg) return;
    const char *src_str = trigger_source_to_str(msg->source);
    const char *type_str = trigger_type_to_str(msg->type);
    liot_trace("OneNet/AEP Task received message: source=%s type=%s", src_str, type_str);
}


