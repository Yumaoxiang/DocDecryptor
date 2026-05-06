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

#include "hal_project.h"


liot_mqtt_client_option aep_mqtt_options = {0};
liot_mqtt_client_t aep_mqtt_cli     = 0;            // 句柄

#define AEP_MQTT_URL      "mqtt://mqtt.ctwing.cn:1883"
#define AEP_MQTT_USER     "Aramis"
#define AEP_MQTT_PASS     "Fh-D_g7LCxKuQZfG7S8qMvyhqCeVa41Xu0qGVfhSQcU"
char aep_mqtt_identity[24] = "17125983000000000000000";  


//格式1：开机
#if PRODUCT_SWITCH
char aep_data_send_poweron[]="{\"product\":\"swt\",\"ver\":\"6.2.1\",\"cmd\":\"Power_On\",\"msgID\":\"BBBBB1\",\"output\":\"1111\",\"iccid\":\"00000000001111111111\",\"pwr\":\"UWN\",\"mem\":\"dflt\",\"workmode\":\"Swt\"}";
#elif PRODUCT_MOTOR
char aep_data_send_poweron[]="{\"product\":\"mot\",\"ver\":\"6.3.0\",\"cmd\":\"Power_On\",\"msgID\":\"BBBBB1\",\"output\":\"1111\",\"iccid\":\"00000000001111111111\",\"pwr\":\"UWN\",\"mem\":\"dflt\",\"workmode\":\"Mot\"}";
#endif

//格式2：单独上报停止定时信息、错误信息、ack信息格式一直，但data长度不同，所以使用2个数组
char aep_data_send_fault[]="{\"cmd\":\"SysFault\",\"data\":\"UnknownCmd\",\"msgID\":\"000000\"}"; 

extern char iot_data_ack[];

// 原始含义	原缩写	含义说明
// 无报警	NON	   No Alarm
// 过压	    OVP	   OverVoltP	Over Voltage Protect
// 欠压	    UVP	   UnderVolt	Under Voltage Protect
// 过流	    OCP	   OverCurrP	Over Current Protect
// 缺相	    PHL	   PhaseLoss	Phase Loss
// 所有报警	ALL	   AllAlarms	All Alarms
// 消除报警	CLR	   ClearAlrm	Clear Alarm
char aep_data_alarm[]="{\"cmd\":\"DevAlarm\",\"type\":\"OVP\",\"param\":\"280.0\",\"msgID\":\"DFLT02\",\"workmode\":\"Swt\"}"; // 设备报警协议，解析type和param字段

//格式3：通用格式
char aep_data_send_all[]="{\"cmd\":\"HeartBet\",\"msgID\":\"BBBBB2\",\"output\":\"0000\",\"netSignal\":\"-100 +00\",\"vbat\":\"0.0V\",\"temp\":\"+25C\",\"workmode\":\"Swt\",\"vol\":\"000.0\",\"cur\":\"000.0\",\"alarm\":\"NON\"}";


/******************************************************************************
*名    称：cat1_init_suc_aep_datasend
*功    能：Cat1初始化成功之后，平台发送数据同步相关信息
*入口参数：type 参考枚举
*返回值：0 失败    1 成功
*说    明： 初始化&心跳，注册成功后，最多发送N次
*修    改： 26-02-15 update_net_signal()在datasend_n_times中已经调用，无需重复调用
*******************************************************************************/
void aep_init_suc_datasend(e_COMMON_UP_COMMAND initCmd){
    adc_vbat_temp_get();
    // aep_update_net_signal();                       // 更新上电后初始网络信号
    datasend_n_times(initCmd, 2);                   // 初始化连接刚刚成功，不需要对发送做异常处理
    if(initCmd == en_CMD_POWERON){                  // 如果是开机
        cat1.pwr_send_flag = 1;                     // 开机后判断是否发送过开机数据
        adc_temp_get();                             // 温度数据第1次异常，重新测试
        datasend_n_times(en_CMD_HEART_BEAT, 2);     // 同时也发送一次心跳数据
    }else if(!cat1.pwr_send_flag){                  // 之前开机数据发送失败
        cat1.pwr_send_flag = 1;                     // 重发开机数据
        datasend_n_times(en_CMD_POWERON, 2);        // 补发开机数据
    }
    liot_trace("========== AEP PowerOn and HeartBeat Send Success ==========");
}


/************************************************************************************
 * 名    称：cat1_update_net_signal
 * 功    能：更新数组对应的网络信号值
 * 说    明：Lierda封装的函数有问题，所以使用原厂appGetECBCInfoSync获取网络信号
************************************************************************************/
void aep_update_net_signal(){
    if(cat1_get_net_signal() == en_CAT1_GET_SIG_SUC){       // 更新网络信号参数
        liot_trace("    update netSignal(pre): %s", aep_data_send_all);
        replaceJson(aep_data_send_all, "netSignal", cat1.netSignal);
        liot_trace("    update netSignal(now): %s", aep_data_send_all);
    }
    else{
        liot_trace("get net signal failed.");
    }
}


/************************************************************************************
 * 名    称：aep_mqtt_deinit
 * 功    能：向服务器发送DISCONNECT请求并注销相关参数
 * 返回值  ：en_CAT1_SUC 成功，其他返回值 失败
 * 说    明：调用liot_mqtt_client_deinit(&aep_mqtt_cli)后需加延时，实测500mS即可
************************************************************************************/
#define AEP_MQTT_DEINIT_WAIT_5S     50
void aep_mqtt_deinit(){
    uint8_t cnt = 0;

    cat1.deinitSucFlag = 0;

    if(liot_mqtt_client_is_connected(&aep_mqtt_cli)){
        liot_trace("liot_mqtt_client_is_connected, disconnect now");
        liot_mqtt_disconnect(&aep_mqtt_cli, mqtt_disconnect_result_cb, NULL);   // 断开MQTT连接
        liot_rtos_task_sleep_s(1);   
    }
    
    liot_mqtt_client_deinit(&aep_mqtt_cli);
    while(!cat1.deinitSucFlag && (cnt<AEP_MQTT_DEINIT_WAIT_5S)){
        cnt++;
        liot_rtos_task_sleep_ms(100);                                       
    }
    aep_mqtt_cli = 0;
    liot_rtos_task_sleep_s(1);
}


/************************************************************************************
 * 名    称：aep_mqtt_reg
 * 功    能：向服务器发送CONNECT请求进行注册
 * 入口参数：
 * 返回值  ：en_CAT1_NET_REG_SUC 成功，其他返回值 失败
 * 说    明：
 * 
 *    1. 待测试：多次初始化
 *    2. 待测试：多次连接
 *    3. 待测试：多次订阅   
 * 订阅服务器下发，回调函数打印信息判断是否订阅成功(后期更改为变量形式)
      问题：如果有多个订阅，如何处理，多次初始化么？
************************************************************************************/
e_CAT1_REG_STATUS aep_mqtt_reg(){
    int ret                         = 0;     
    int cid                         = 1;
    uint8_t aep_reg_cnt = 0;                    // 超时计数变量
    
    liot_trace("========== AEP MQTT Register Start ==========");     
    aep_mqtt_reg_identity_strcat();
    aep_mqtt_reg_option_init();
    if (liot_mqtt_client_init(&aep_mqtt_cli, cid) != LIOT_MQTTCLIENT_SUCCESS){
        liot_trace("mqtt client init failed!!!!");
        return en_CAT1_FAIL;
    }

    liot_trace("========== AEP MQTT Connect Start ==========");     
    cat1.reg_flag = 0;                                          // 根据该标志位判断是否注册成功
    ret = liot_mqtt_connect(&aep_mqtt_cli, AEP_MQTT_URL, mqtt_connect_result_cb, NULL, (liot_mqtt_client_option *)&aep_mqtt_options, mqtt_state_exception_cb);
    while((cat1.reg_flag == 0) && (aep_reg_cnt < 15)){          // 最长等待16.5S，底层最长要30S才能返回
        aep_reg_cnt++;
        liot_trace("aep_mqtt_reg connect %d times", aep_reg_cnt);   // 周期打印
        liot_rtos_task_sleep_ms(1000);                              // 尝试周期1S/次
        LED_NET_ON_10MS;
        liot_rtos_task_sleep_ms(100);
        LED_NET_ON_10MS;
    }
    if (cat1.reg_flag == 0){
        liot_trace("mqtt connect failed!!!, ret = %d", ret);
        return en_CAT1_FAIL;
    }
    liot_trace("mqtt connect success!");

    // 进行订阅
    cat1.reg_flag = 0;                                          // 根据该标志位判断是否注册成功
    aep_reg_cnt = 0;
    liot_trace("========== AEP MQTT Subscribe Start ==========");    
    if(liot_mqtt_sub_unsub(&aep_mqtt_cli, "device_control", 1, mqtt_requst_result_cb, NULL, 1) == LIOT_MQTTCLIENT_WOUNDBLOCK){
        while((cat1.reg_flag == 0) && (aep_reg_cnt < 12)){      // 最长等待6S
            liot_rtos_task_sleep_ms(500);                       // 等待周期0.5S/次
            liot_trace("aep_mqtt_reg sub wait 500ms %d times", aep_reg_cnt);
            cat1_reg_led_check(en_CAT1_LED_CLOUD_REG);          // 中间穿插LED闪烁
        }
        if (cat1.reg_flag == 0){
            liot_trace("mqtt sub failed!!!, ret = %d", ret);
            return en_CAT1_FAIL;
        }
        liot_trace("mqtt sub success!");
    }

    // 设置订阅回调函数
    liot_mqtt_set_inpub_callback(&aep_mqtt_cli, mqtt_inpub_data_cb, NULL);      // 定义订阅回调函数

    return en_CAT1_NET_REG_SUC;
}


/************************************************************************************
 * 名    称：aep_mqtt_reg_identity_strcat
 * 功    能：拼接AEP注册时的认证字符串，一型一密固定为："产品ID"+"IMEI"
 * 说    明：
************************************************************************************/
void aep_mqtt_reg_identity_strcat(){
    liot_trace("aep_mqtt_reg_identity_strcat start...");
    liot_trace("  1.aep_mqtt_identity is %s", aep_mqtt_identity);
    liot_trace("  2.IMEI is %s", cat1.imei);
    for(uint8_t i = 0; i < 15; i++){
        aep_mqtt_identity[i+8] = cat1.imei[i]; 
    }
    liot_trace("  3.aep_mqtt_identity is %s", aep_mqtt_identity);
}


/************************************************************************************
 * 名    称：aep_mqtt_reg_option_init
 * 功    能：配置MQTT基本参数
 * 说    明：
************************************************************************************/
void aep_mqtt_reg_option_init(){
    aep_mqtt_options.version = LIOT_MQTT_VERSION_4;          // MQTT 3.1.1
    aep_mqtt_options.pdp_cid = 1;                            // OpenCPU固定为'1'，标准AT透传为'0'
    aep_mqtt_options.ssl_enable = false,
    aep_mqtt_options.ssl_cfg       = NULL;
    aep_mqtt_options.clean_session = 1;                      // 关闭MQTT会话复用机制，每次connect都是一次新的session，会话仅持续和网络同样长的时间()
    aep_mqtt_options.kalive_time   = 300;                    // AEP默认支持5min，这里与平台保持一致
    aep_mqtt_options.delivery_time = 5;                      // 发送超时时间，单位：秒
    aep_mqtt_options.delivery_cnt  = 3;                      // 发送超时后重发次数
    aep_mqtt_options.will_flag     = 0;                      // 遗嘱消息控制，AEP不支持遗嘱
    aep_mqtt_options.will_qos      = 0;
    aep_mqtt_options.will_retain   = 0;
    aep_mqtt_options.ping_timeout  = 5;                      // ping包超时时间，实际功能需确认
    aep_mqtt_options.client_id     = aep_mqtt_identity;
    aep_mqtt_options.client_user   = AEP_MQTT_USER;
    aep_mqtt_options.client_pass   = AEP_MQTT_PASS;
    memset(aep_mqtt_options.will_topic, 0x00, 256);
    memset(aep_mqtt_options.will_message, 0x00, 256);
}


/*******************************************************************************
*名    称：aep_datasend
*功    能：发送1次数据
*入口参数：cmd-发送不同的命令    
          param-个别cmd下面细分param参数
*返回值：0-失败  1-成功收到平台数据
*说    明：
*修    改：22-10-19 增加param参数，sysFault时可以区分哪种Fault
*******************************************************************************/
uint8_t aep_datasend(e_COMMON_UP_COMMAND cmd){
    uint8 cntTime = 0;
    cat1.pub_suc_flag = 0;
    // 格式1：开机
    if(cmd == en_CMD_POWERON){
        aep_datasend_fillproto_poweron();
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB", aep_data_send_poweron, strlen(aep_data_send_poweron), 1, 0, mqtt_requst_result_cb, NULL);
    }
    // 格式2：报警
    else if(cmd == en_CMD_DEV_ALARM){
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB", aep_data_alarm, strlen(aep_data_alarm), 1, 0, mqtt_requst_result_cb, NULL);
    }
    // 格式3：通用(心跳、本地开关、定时开关)
    else{
        aep_datasend_fillproto(cmd);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB", aep_data_send_all, strlen(aep_data_send_all), 1, 0, mqtt_requst_result_cb, NULL);
    }
    
    while((cat1.pub_suc_flag==0) && (cntTime < 80)){
        cntTime++;
        liot_rtos_task_sleep_ms(100);               // 每包数据最长等待8S
    }
    
    if(cat1.pub_suc_flag) return 1;
    return 0;
}


/*******************************************************************************
*名    称：aep_datasend_fillproto_poweron()
*功    能：系统上电发送第1包数据
*说    明：固定填充
*修    改：24-09-28 增加pwr替换
*******************************************************************************/
void aep_datasend_fillproto_poweron(){
    // Step 0: 确认是否为平台主动获取上电数据
    if(cat1.rcv.poweron_info_flag_get){
        cat1.rcv.poweron_info_flag_get = 0;  // 清除标志位
        replaceJson(aep_data_send_poweron, "cmd", "ModlInfo");
        replaceJson(aep_data_send_poweron, "msgID", cat1.rcv.SerRandomID);  // 替换msgID
    }else{
        replaceJson(aep_data_send_poweron, "cmd", "Power_On");
        replaceJson(aep_data_send_poweron, "msgID", "BBBBB1");              // 替换msgID
    }

    datasend_fillproto_common_replace_output(en_CMD_POWERON);

    // Step 2：替换iccid
    replaceJson(aep_data_send_poweron, "iccid", cat1.iccid);
    
    // Step 3：替换开机原因
    replaceJson(aep_data_send_poweron, "pwr", cat1.pwrReason);
#if PRODUCT_SWITCH
    // Step 4：替换Flash读取上电状态 状态记忆 调试仿真 26-03-02 可以统一记忆到模组，只是根据宏定义是否推送MCU
    #if MEM_STATE_MCU_FLASH
    replaceJson(aep_data_send_poweron, "mem", "Keep");
    #else
    if(strcmp(flash_poweron.mode, "off") == 0){
        replaceJson(aep_data_send_poweron, "mem", "Clos");
    }
    else if(strcmp(flash_poweron.mode, "o_n") == 0){
        replaceJson(aep_data_send_poweron, "mem", "Open");
    }
    else if(strcmp(flash_poweron.mode, "pre") == 0){
        replaceJson(aep_data_send_poweron, "mem", "Hold");
    }
    #endif
#endif
    liot_trace("aep_data_send_poweron[]: %s", aep_data_send_poweron);
}


/*******************************************************************************
*名    称：DataSend_Command_FillProto()
*功    能：NB上法数据--填充协议格式
*入口参数：cmd -> 对应指令命令 
          msgType  -> 每个命令匹配对应msgID类型
          faultParam -> 设备异常填充更详细参数
*出口参数：
*说    明：1. 发送字节长度固定
           2.  cmd字符串必须是固定7位
*修    改：23-05-13 入口参数增加msgID
*******************************************************************************/
void aep_datasend_fillproto(e_COMMON_UP_COMMAND com){
    // Step 1: 更新output、workmode
    datasend_fillproto_common_replace_output(com);
    liot_trace("aep_data_send_all[] replace 'output' & 'workmode': %s", aep_data_send_all);

    // Step 3: 更新专用信息(cmd、msgID、网络信号)
    switch(com){
    case en_CMD_HEART_BEAT:
        aep_update_net_signal();
        replaceJson(aep_data_send_all, "cmd", "HeartBet");
        replaceJson(aep_data_send_all, "msgID", "SYS_HB");  //这里针对msgID进行特殊处理25-06-15
        replaceJson(aep_data_send_all, "vbat", cat1.vbat);
        replaceJson(aep_data_send_all, "temp", cat1.temp);
        replaceJson(aep_data_send_all, "vol", cat1.param.voltage);
        replaceJson(aep_data_send_all, "cur", cat1.param.current);
        replaceJson(aep_data_send_all, "alarm", cat1.param.alarm);
        break;
    case en_CMD_LOCAL_SWITCH:
        replaceJson(aep_data_send_all, "cmd", "LocSwich");
        replaceJson(aep_data_send_all, "msgID", cat1.msgIDStr);
        break;
    case en_CMD_UART_SWITCH_IO:
        replaceJson(aep_data_send_all, "cmd", "LocSwich");
        replaceJson(aep_data_send_all, "msgID", "UART_4");
        break;
    case en_CMD_UART_SWITCH_PARAM_UPDATE:
        replaceJson(aep_data_send_all, "cmd", "ParamUpd");
        replaceJson(aep_data_send_all, "msgID", "UART_5");
        replaceJson(aep_data_send_all, "vol", cat1.param.voltage);
        replaceJson(aep_data_send_all, "cur", cat1.param.current);
        replaceJson(aep_data_send_all, "alarm", cat1.param.alarm);
        break;
    case en_CMD_VOL_AND_CUR_ACK:
        replaceJson(aep_data_send_all, "cmd", "ParamUpd");
        replaceJson(aep_data_send_all, "msgID", cat1.rcv.SerRandomID);
        replaceJson(aep_data_send_all, "vol", cat1.param.voltage);
        replaceJson(aep_data_send_all, "cur", cat1.param.current);
        replaceJson(aep_data_send_all, "alarm", cat1.param.alarm);
        break;
    // case en_CMD_DEV_ALARM:
    //     break;

    case en_CMD_UART_MOTOR_IO:
        replaceJson(aep_data_send_all, "cmd", "LocSwich");
        replaceJson(aep_data_send_all, "msgID", "UART_7");
        break;
    case en_CMD_UART_MOTOR_PARAM_UPDATE:
        replaceJson(aep_data_send_all, "cmd", "ParamUpd");
        replaceJson(aep_data_send_all, "msgID", "UART_8");
        replaceJson(aep_data_send_all, "vol", cat1.param.voltage);
        replaceJson(aep_data_send_all, "cur", cat1.param.current);
        replaceJson(aep_data_send_all, "alarm", cat1.param.alarm);
        break;
    default:break;
    }
    liot_trace("aep_data_send_all[] replace 'cmd' & 'msgID': %s", aep_data_send_all);
}


//-------------------------------------↑--------------------------------------
//------------------------------------上行------------------------------------
//------------------------------------下行------------------------------------
//-------------------------------------↓--------------------------------------

/*******************************************************************************
*名    称：aep_datasend_ack
*功    能：平台下发数据，模组反馈包
*入口参数：cmd-发送不同的命令    
*返回值：0-反馈包发送失败  1-反馈包发送成功
*说    明：
*修    改：22-10-19 增加param参数，sysFault时可以区分哪种Fault
*******************************************************************************/
uint8_t aep_datasend_ack(e_IOT_DOWN_COMMAND cmd){
    uint8 cntTime = 0;
    
    cat1.pub_suc_flag = 0;
    switch(cmd){
    case en_CMD_REMOTE_SWITCH:
        liot_trace("en_CMD_REMOTE_SWITCH ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "RemoteAck");
        iot_ack_remote_switch_replace_data_workmode();
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_REMOTE_SWITCH ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_POWERON_STATUS_CLOS:
        liot_trace("en_CMD_POWERON_STATUS_CLOS ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Clos");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_CLOS ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_POWERON_STATUS_OPEN:
        liot_trace("en_CMD_POWERON_STATUS_OPEN ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Open");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_OPEN ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_POWERON_STATUS_HOLD:
        liot_trace("en_CMD_POWERON_STATUS_HOLD ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Hold");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_HOLD ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_POWERON_STATUS_FAIL:
        liot_trace("en_CMD_POWERON_STATUS_FAIL ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Fail");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_FAIL ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_FOTA_START: 
        liot_trace("en_CMD_FOTA_START ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "DiFotaAck");
        replaceJson(iot_data_ack, "data", "Stat");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FOTA_START ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;
    case en_CMD_FOTA_ERROR_ADRESS: 
        liot_trace("en_CMD_FOTA_ERROR_ADRESS ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "DiFotaAck");
        replaceJson(iot_data_ack, "data", "ErrA");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FOTA_ERROR_ADRESS ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;
    case en_CMD_FOTA_ERROR_LENTH: 
        liot_trace("en_CMD_FOTA_ERROR_LENTH ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "DiFotaAck");
        replaceJson(iot_data_ack, "data", "ErrL");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FOTA_ERROR_LENTH ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;
        
    case en_CMD_FLASH_ERROR_WRITE: 
        liot_trace("en_CMD_FLASH_ERROR_WRITE ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "ErrW");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FLASH_ERROR_WRITE ack(now): %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_SET_LIMIT_SUCC:
        replaceJson(iot_data_ack, "cmd", "SetLimAck");
        replaceJson(iot_data_ack, "data", "Succ");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_SET_LIMIT_SUCC ACK is: %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_SET_LIMIT_FAIL: 
        replaceJson(iot_data_ack, "cmd", "SetLimAck");
        replaceJson(iot_data_ack, "data", "Fail");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_SET_LIMIT_FAIL ACK is: %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_CMD_POWERON_LINK_TEST: 
        replaceJson(iot_data_ack, "cmd", "LinkT_Ack");
        replaceJson(iot_data_ack, "data", "Succ");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_LINK_TEST ACK is: %s", iot_data_ack);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_ACK", iot_data_ack, strlen(iot_data_ack), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    default:
        liot_trace("[info] unknown command, could not ack to AEP.");
        return 0;
    }
    
    while(cat1.pub_suc_flag && (cntTime < 80)){
        cntTime++;
        liot_rtos_task_sleep_ms(100);               // 每包数据最长等待8S
    }
    
    if(cat1.pub_suc_flag) return 1;
    return 0;
}


/*******************************************************************************
*名    称：aep_datasend_fault
*功    能：发送1次数据
*入口参数：cmd-对应不同的错误命令    
*返回值：0-失败  1-成功收到平台数据
*说    明：
*修    改：
*******************************************************************************/
uint8_t aep_datasend_fault(e_FAULT_TYPE cmd){
    uint8 cntTime = 0;

    cat1.pub_suc_flag = 0;
    switch(cmd){
    case en_FAULT_CMD:
        liot_trace("aep_datasend_fault: unknown command.");
        replaceJson(aep_data_send_fault, "data", "UnknownCmd");
        replaceJson(aep_data_send_fault, "msgID", "FFFFFF");
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;
    case en_FAULT_REMOTE_IO:
        liot_trace("aep_datasend_fault: remote IO output.");
        replaceJson(aep_data_send_fault, "data", "Server_Out");
        replaceJson(aep_data_send_fault, "msgID", cat1.rcv.SerRandomID);
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;    
    case en_FAULT_UART_RCV_OVERFLOW:
        liot_trace("aep_datasend_fault: UART receive data overflow");
        replaceJson(aep_data_send_fault, "data", "UartRcvLen");
        replaceJson(aep_data_send_fault, "msgID", "FFFFFE");
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;
    case en_FAULT_UART_CMD_FREQ_OVERFLOW:
        liot_trace("aep_datasend_fault: UART cmd freq overflow");
        replaceJson(aep_data_send_fault, "data", "UartCmdFre");
        replaceJson(aep_data_send_fault, "msgID", "FFFFFD");
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_FAULT_OUT1_STATUS:
        replaceJson(aep_data_send_fault, "data", "Out1StaErr");
        replaceJson(aep_data_send_fault, "msgID", "StaErr");
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    case en_FAULT_MOTOR_SER_STATUS_DISMATCH:
        liot_trace("aep_datasend_fault: Server Motor status dismatch");
        replaceJson(aep_data_send_fault, "data", "mStaDismch");
        replaceJson(aep_data_send_fault, "msgID", cat1.rcv.SerRandomID);
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;
    case en_FAULT_MOTOR_SER_STATUS_LOGIC:
        liot_trace("aep_datasend_fault: Server Motor logic error");
        replaceJson(aep_data_send_fault, "data", "mLogicErro");
        replaceJson(aep_data_send_fault, "msgID", cat1.rcv.SerRandomID);
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;
    case en_FAULT_MOTOR_SER_STATUS_REPEAT:
        liot_trace("aep_datasend_fault: Server Motor logic error");
        replaceJson(aep_data_send_fault, "data", "mLogicRept");
        replaceJson(aep_data_send_fault, "msgID", cat1.rcv.SerRandomID);
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;
     case en_FAULT_MOTOR_SER_STATUS_UNKNOWN:
        liot_trace("aep_datasend_fault: Server Motor logic unknown");
        replaceJson(aep_data_send_fault, "data", "mLogicUnkw");
        replaceJson(aep_data_send_fault, "msgID", cat1.rcv.SerRandomID);
        liot_trace("aep_data_send_fault: %s", aep_data_send_fault);
        liot_mqtt_publish(&aep_mqtt_cli, "NT26KCNB_FAULT", aep_data_send_fault, strlen(aep_data_send_fault), 1, 0, mqtt_requst_result_cb, NULL);
        break;

    default:
        liot_trace("[info] unknown fault command, could not send to AEP.");
        return 0;
    }
    
    while(cat1.pub_suc_flag && (cntTime < 80)){
        cntTime++;
        liot_rtos_task_sleep_ms(100);               // 每包数据最长等待8S
    }
    
    if(cat1.pub_suc_flag) return 1;
    return 0;
}
