#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>             // 提取服务器下发的字符串
#include <ctype.h>              // 提取服务器下发的字符串


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



liot_mqtt_client_option onenet_mqtt_options = {0};
liot_mqtt_client_t onenet_mqtt_cli     = 0;            // 句柄


#define ONENET_CLIENT_URL       "mqtt://mqtts.heclouds.com:1883"                // onenet 的ip地址

#define MQTT_CLIENT_USER        "6Q2HzvtYGx"                                    //产品ID
#define MQTT_CLIENT_IDENTITY    "860889089802610"
#define MQTT_CLIENT_PASS        "k5nBX8tB1QH3VVFBV5kVZfmb8T8FI+W1UfelgdOFbwk="
// #define MQTT_PRODUCT_TOKEN      "version=2018-10-31&res=products%2F6Q2HzvtYGx&et=2147483160&method=md5&sign=xJmY0AHOyPNFwaOjSzDeCw%3D%3D"          // 产品密钥
#define ONENET_FIXED_TOKEN      "version=2018-10-31&res=products%2F6Q2HzvtYGx&et=2147483160&method=md5&sign=xJmY0AHOyPNFwaOjSzDeCw%3D%3D"

// 1. 通用订阅、发布，此处不同的命令格式都一致
// #define ONENET_SUB_DOWNLINK_TOPIC   "$sys/"MQTT_CLIENT_USER"/"MQTT_CLIENT_IDENTITY"/thing/service/Downlink/invoke"
static char onenet_sub_downlink_topic[128] = {0};                               //订阅下行主题，唯一
static char onenet_invoke_reply_topic[128] = {0};                               //下行反馈主题，唯一
// #define MQTT_ONENET_PUB_TOPIC   "$sys/" MQTT_CLIENT_USER "/" MQTT_CLIENT_IDENTITY "/thing/event/post"
static char onenet_pub_topic[128] = {0};                                        //发布上行主题，唯一


static int onenet_pub_id = 100;                                                 // 循环 id，初始与原示例相同，100~999之间循环

// 2. 不同命令的发布内容不同，命名规则中，"payload"主题类总串，"data"是其子集 
static char onenet_payload_poweron[512] = {0};                                  // 运行时构造的发布内容
char onenet_data_poweron[]="{\"product\":\"swt\",\"ver\":\"6.3.1\",\"cmd\":\"Power_On\",\"msgID\":\"BBBBB0\",\"output\":\"0000\",\"iccid\":\"00000000001111111111\",\"pwr\":\"UWN\",\"mem\":\"dflt\",\"workmode\":\"Swt\",\"rev\":\"null\"}";

static char onenet_payload_common[512] = {0};                                   // 通用协议内容
char onenet_data_common[]="{\"cmd\":\"HeartBet\",\"msgID\":\"BBBBB2\",\"output\":\"0000\",\"netSignal\":\"-100 +00\",\"vbat\":\"0.0V\",\"temp\":\"+25C\",\"vol\":\"000.0\",\"cur\":\"000.0\",\"rev\":\"null\"}";

static char onenet_payload_fault[256] = {0};                                    // 设备异常协议内容
char onenet_data_fault[]="{\"cmd\":\"SysFault\",\"data\":\"UnknownCmd\",\"msgID\":\"000000\"}"; 

static char onenet_payload_ack[256] = {0};                                      // 远程控制应答协议内容
extern char iot_data_ack[];


/**
 * @brief MQTT重连处理函数
 * 当MQTT连接丢失时，此函数将被调用。它会输出一条跟踪信息，提示正在尝试重新连接。开发者可以在此函数中添加重连时的自定义处理逻辑。
 * @param client 指向MQTT客户端实例的指针
 * @param arg 传递给回调函数的参数
 */
static void mqtt_reconnect_handler(liot_mqtt_client_t *client, void *arg)
{
    liot_trace("trying reconnect...");
    // 在这里可以添加重连时的处理逻辑
}


/******************************************************************************
*名    称：cat1_init_suc_onenet_datasend
*功    能：Cat1初始化成功之后，平台发送数据同步相关信息
*入口参数：type 参考枚举
*返回值：0 失败    1 成功
*说    明： 初始化&心跳，注册成功后，最多发送N次
*修    改： 26-02-15 update_net_signal()在datasend_n_times中已经调用，无需重复调用
*******************************************************************************/
void onenet_init_suc_datasend(e_COMMON_UP_COMMAND initCmd){
    adc_vbat_temp_get(); 
    // onenet_update_net_signal();                     // 更新当前网络信号
    datasend_n_times(initCmd, 2);                   // 初始化连接刚刚成功，不需要对发送做异常处理
    if(initCmd == en_CMD_POWERON){                  // 如果是开机
        cat1.pwr_send_flag = 1;                     // 开机后判断是否发送过开机数据
        adc_temp_get();                             // 温度数据第1次异常，重新测试
        datasend_n_times(en_CMD_HEART_BEAT, 2);     // 同时也发送一次心跳数据
    }else if(!cat1.pwr_send_flag){                  // 之前开机数据发送失败
        cat1.pwr_send_flag = 1;                     // 重发开机数据
        datasend_n_times(en_CMD_POWERON, 2);        // 补发开机数据
    }
    liot_trace("========== OneNet PowerOn and HeartBeat Send Success ==========");     
}


/************************************************************************************
 * 名    称：onenet_mqtt_reg_option_init
 * 功    能：配置MQTT基本参数
 * 说    明：
************************************************************************************/
void onenet_mqtt_reg_option_init(){
    onenet_mqtt_options.version = LIOT_MQTT_VERSION_4;          // MQTT 3.1.1
    onenet_mqtt_options.pdp_cid = 1;                            // OpenCPU固定为'1'，标准AT透传为'0'
    onenet_mqtt_options.ssl_enable = false,
    onenet_mqtt_options.ssl_cfg       = NULL;
    onenet_mqtt_options.clean_session = 1;                      // 关闭MQTT会话复用机制，每次connect都是一次新的session，会话仅持续和网络同样长的时间()
    onenet_mqtt_options.kalive_time   = 300;                    // AEP默认支持5min，这里与平台保持一致
    onenet_mqtt_options.delivery_time = 5;                      // 发送超时时间，单位：秒
    onenet_mqtt_options.delivery_cnt  = 3;                      // 发送超时后重发次数
    onenet_mqtt_options.will_flag     = 0;                      // 遗嘱消息控制，AEP不支持遗嘱
    onenet_mqtt_options.will_qos      = 0;
    onenet_mqtt_options.will_retain   = 0;
    onenet_mqtt_options.ping_timeout  = 5;                      // ping包超时时间，实际功能需确认
    onenet_mqtt_options.client_id     = cat1.imei;
    onenet_mqtt_options.client_user   = MQTT_CLIENT_USER;
    onenet_mqtt_options.client_pass   = MQTT_CLIENT_PASS;
    memset(onenet_mqtt_options.will_topic, 0x00, 256);
    memset(onenet_mqtt_options.will_message, 0x00, 256);
}


/************************************************************************************
 * 名    称：aep_mqtt_deinit
 * 功    能：向服务器发送DISCONNECT请求并注销相关参数
 * 返回值  ：en_CAT1_SUC 成功，其他返回值 失败
 * 说    明：调用liot_mqtt_client_deinit(&aep_mqtt_cli)后需加延时，实测500mS即可
************************************************************************************/
#define ONENET_MQTT_DEINIT_WAIT_5S     50
void onenet_mqtt_deinit(){
    uint8_t cnt = 0;

    cat1.deinitSucFlag = 0;
    if(liot_mqtt_client_is_connected(&onenet_mqtt_cli)){
        liot_trace("liot_mqtt_client_is_connected, disconnect now");
        liot_mqtt_disconnect(&onenet_mqtt_cli, mqtt_disconnect_result_cb, NULL);   // 断开MQTT连接
        liot_rtos_task_sleep_s(1);   
    }
    
    liot_mqtt_client_deinit(&onenet_mqtt_cli);
    while(!cat1.deinitSucFlag && (cnt<ONENET_MQTT_DEINIT_WAIT_5S)){
        cnt++;
        liot_rtos_task_sleep_ms(100);                                       
    }
    onenet_mqtt_cli = 0;
    liot_rtos_task_sleep_s(1);
}

/************************************************************************************
 * 名    称：onenet_mqtt_reg
 * 功    能：向服务器发送CONNECT请求进行注册
 * 入口参数：
 * 返回值  ：en_CAT1_NET_REG_SUC 成功，其他返回值 失败
 * 说    明：
 * 
 *    1. 待测试：多次初始化
 *    2. 待测试：多次连接
 *    3. 待测试：多次订阅   
 * 修    改：25-10-13 替换设备级别的token计算为产品级别固定token，避免每次都计算
************************************************************************************/
e_CAT1_REG_STATUS onenet_mqtt_reg(){
    int ret                         = 0;     
    int cid                         = 1;
    uint8_t onenet_reg_cnt = 0;                     // 超时计数变量


    onenet_mqtt_reg_option_init();                     // 初始化MQTT参数
    
    onenet_mqtt_options.client_pass = ONENET_FIXED_TOKEN;
    liot_trace("token: %s", onenet_mqtt_options.client_pass);

    liot_trace("========== OneNet MQTT Register Start ==========");     
    

    if (liot_mqtt_client_init(&onenet_mqtt_cli, cid) != LIOT_MQTTCLIENT_SUCCESS){
        liot_trace("onenet client init failed!!!!");
        return en_CAT1_FAIL;
    }

    liot_mqtt_set_reconnect_callback(&onenet_mqtt_cli, mqtt_reconnect_handler, NULL);

    liot_trace("========== OneNet MQTT Connect Start ==========");     

    cat1.reg_flag = 0;                                          // 根据该标志位判断是否注册成功
    ret = liot_mqtt_connect(&onenet_mqtt_cli, ONENET_CLIENT_URL, mqtt_connect_result_cb, NULL, (liot_mqtt_client_option *)&onenet_mqtt_options, mqtt_state_exception_cb);
    while((cat1.reg_flag == 0) && (onenet_reg_cnt < 15)){          // 最长等待16.5S，底层最长要30S才能返回
        onenet_reg_cnt++;
        liot_trace("onenet_mqtt_reg connect %d times", onenet_reg_cnt);   // 周期打印
        liot_rtos_task_sleep_ms(1000);                              // 尝试周期1S/次
        LED_NET_ON_10MS;
        liot_rtos_task_sleep_ms(80);
        LED_NET_ON_10MS;

        #if UART_PROTOCOL_FORMAT_ASCII
        drv_send_cmd3_push_net_status();                        // 通过UART发送当前注册状态
        #endif
    }
    if (cat1.reg_flag == 0){
        liot_trace("mqtt connect failed!!!, ret = %d", ret);
        return en_CAT1_FAIL;
    }
    liot_trace("mqtt connect success!");


    // 设置订阅回调函数
    liot_mqtt_set_inpub_callback(&onenet_mqtt_cli, mqtt_inpub_data_cb, NULL);      // 定义订阅回调函数

    // 进行订阅
    liot_trace("========== OneNet MQTT Subscribe Start ==========");    
    e_CAT1_REG_STATUS sub_status = onenet_mqtt_sub_topic();
    if(sub_status != en_CAT1_NET_SUB_SUC){
        liot_trace("onenet mqtt subscribe failed!!!");
        return en_CAT1_FAIL;
    }

    liot_trace("========== OneNet MQTT Register Success ==========");
    return en_CAT1_NET_REG_SUC;
}

/************************************************************************************
 * 名    称：onenet_mqtt_sub_topic
 * 功    能：订阅Onenet指定主题
 * 说    明：
 * **********************************************************************************/
e_CAT1_REG_STATUS onenet_mqtt_sub_topic(){
    uint8_t onenet_sub_cnt = 0;                     // 超时计数变量

    snprintf(onenet_sub_downlink_topic, sizeof(onenet_sub_downlink_topic),
            "$sys/%s/%s/thing/service/Downlink/invoke",
            MQTT_CLIENT_USER, cat1.imei);
    liot_trace("onenet_sub_downlink_topic: %s", onenet_sub_downlink_topic);
    

    if(liot_mqtt_sub_unsub(&onenet_mqtt_cli, onenet_sub_downlink_topic, 1, mqtt_requst_result_cb, NULL, 1) == LIOT_MQTTCLIENT_WOUNDBLOCK){
        cat1.sub_flag = 0;                                          // 根据该标志位判断是否注册成功
        onenet_sub_cnt = 0;
        while((cat1.sub_flag == 0) && (onenet_sub_cnt < 12)){       // 最长等待6S
            onenet_sub_cnt++;                                       // 超时计数变量
            liot_trace("subtopic begin: cat1.sub_flag=%d, onenet_sub_cnt=%d", cat1.sub_flag, onenet_sub_cnt);
            liot_rtos_task_sleep_ms(500);                           // 尝试周期0.5S/次
            // cat1_reg_led_check(en_CAT1_LED_AEP_REG);             // 26-01-31 删除LED指示，因为在PowerOn之后和心跳中调用，会将指示灯熄灭
        }
        liot_trace("subtopic end: cat1.sub_flag=%d, onenet_sub_cnt=%d", cat1.sub_flag, onenet_sub_cnt);
        
        if (cat1.sub_flag == 0){
            liot_trace("===> mqtt sub failed!!!");
            return en_CAT1_FAIL;
        }
        liot_trace("onenet_sub_downlink_topic success!");
    }

    return en_CAT1_NET_SUB_SUC;
}

/*******************************************************************************
*名    称：onenet_datasend
*功    能：发送1次数据
*入口参数：cmd-发送不同的命令    
*返回值：0-失败  1-成功收到平台数据
*说    明：
*修    改：
*******************************************************************************/
uint8_t onenet_datasend(e_COMMON_UP_COMMAND cmd){
    uint8 cntTime = 0;
    cat1.pub_suc_flag = 0;

    
    /* 构造发布主题，使用模组的 IMEI */
    snprintf(onenet_pub_topic, sizeof(onenet_pub_topic),
            "$sys/%s/%s/thing/event/post",
            MQTT_CLIENT_USER, cat1.imei);

    /* 构造动态 payload：id 在 100-999 循环，value 使用 onenet_data_poweron 的 JSON */
    onenet_pub_id++;
    if (onenet_pub_id > 999) onenet_pub_id = 100;

    // 格式1：开机
    if(cmd == en_CMD_POWERON){
        onenet_datasend_fillproto_poweron();

        snprintf(onenet_payload_poweron, sizeof(onenet_payload_poweron),
                "{\"id\": \"%d\",\"version\": \"1.0\",\"params\": {\"DevPwrOn\": {\"value\": %s}}}",
                onenet_pub_id, onenet_data_poweron);

        liot_mqtt_publish(&onenet_mqtt_cli, onenet_pub_topic, onenet_payload_poweron, strlen(onenet_payload_poweron), 1, 0, mqtt_requst_result_cb, NULL);

        liot_trace("publish topic length: %d, topic data: %s", strlen(onenet_pub_topic), onenet_pub_topic);
        liot_trace("publish payload length: %d, payload data: %s", strlen(onenet_payload_poweron), onenet_payload_poweron);
    }

    // 格式2：通用(心跳、本地开关、数据等)
    else{
        onenet_datasend_fillproto_common(cmd);

        snprintf(onenet_payload_common, sizeof(onenet_payload_common),
                "{\"id\": \"%d\",\"version\": \"1.0\",\"params\": {\"Common\": {\"value\": %s}}}",
                onenet_pub_id, onenet_data_common);

        liot_mqtt_publish(&onenet_mqtt_cli, onenet_pub_topic, onenet_payload_common, strlen(onenet_payload_common), 1, 0, mqtt_requst_result_cb, NULL);

        liot_trace("publish topic length: %d, topic data: %s", strlen(onenet_pub_topic), onenet_pub_topic);
        liot_trace("publish payload length: %d, payload data: %s", strlen(onenet_payload_common), onenet_payload_common);
    }


    while((cat1.pub_suc_flag==0) && (cntTime < 80)){
        cntTime++;
        liot_rtos_task_sleep_ms(100);               // 每包数据最长等待8S
    }
    

    if(cat1.pub_suc_flag) return 1;
    return 0;
}

/*******************************************************************************
*名    称：onenet_datasend_fillproto_poweron()
*功    能：系统上电发送第1包数据
*说    明：固定填充
*修    改：
*******************************************************************************/
void onenet_datasend_fillproto_poweron(){
    // Step 1: 确认是否为平台主动获取上电数据
    if(cat1.rcv.poweron_info_flag_get){
        cat1.rcv.poweron_info_flag_get = 0;  // 清除标志位
        replaceJson(onenet_data_poweron, "cmd", "ModlInfo");
        replaceJson(onenet_data_poweron, "msgID", cat1.rcv.SerRandomID);  // 替换msgID
    }else{
        replaceJson(onenet_data_poweron, "cmd", "Power_On");
        replaceJson(onenet_data_poweron, "msgID", "BBBBB1");              // 替换msgID
    }

    // Step 2: 更新通用信息 "输出状态"
    datasend_fillproto_common_replace_output(en_CMD_POWERON);

    // Step 3：替换iccid
    replaceJson(onenet_data_poweron, "iccid", cat1.iccid);
    
    // Step 4：替换开机原因
    replaceJson(onenet_data_poweron, "pwr", cat1.pwrReason);


    // Step 5：替换Flash读取上电状态 状态记忆
    #if MEM_STATE_MCU_FLASH
    replaceJson(onenet_data_poweron, "mem", "Keep");
    #else
    if(strcmp(flash_poweron.mode, "off") == 0){
        replaceJson(onenet_data_poweron, "mem", "Clos");
    }
    else if(strcmp(flash_poweron.mode, "o_n") == 0){
        replaceJson(onenet_data_poweron, "mem", "Open");
    }
    else if(strcmp(flash_poweron.mode, "pre") == 0){
        replaceJson(onenet_data_poweron, "mem", "Hold");
    }
    #endif

    liot_trace("onenet_data_poweron is: %s", onenet_data_poweron);
}


/*******************************************************************************
*名    称：onenet_datasend_fillproto_common()
*功    能：填充通用类协议格式
*入口参数：cmd -> 对应指令命令 
*说    明：
*修    改：
*******************************************************************************/
void onenet_datasend_fillproto_common(e_COMMON_UP_COMMAND com){
    // Step 1: 更新output、workmode
    datasend_fillproto_common_replace_output(com);

    // Step 3: 更新专用信息(cmd、网络信号等)
    switch(com){
    case en_CMD_HEART_BEAT:
        onenet_update_net_signal();
        replaceJson(onenet_data_common, "cmd", "HeartBet");
        replaceJson(onenet_data_common, "msgID", "SYS_HB");     //这里针对msgID进行特殊处理25-06-15
        replaceJson(onenet_data_common, "vbat", cat1.vbat);
        replaceJson(onenet_data_common, "temp", cat1.temp);
        break;
    case en_CMD_LOCAL_SWITCH:
        replaceJson(onenet_data_common, "cmd", "LocSwich");
        replaceJson(onenet_data_common, "msgID", cat1.msgIDStr);
        break;
    case en_CMD_UART_SWITCH_IO:
        replaceJson(onenet_data_common, "cmd", "LocSwich");
        replaceJson(onenet_data_common, "msgID", "UART_4");     //UART IO数据更新，在UART中为'cmd4'命令
        break;
    case en_CMD_UART_SWITCH_PARAM_UPDATE:
        replaceJson(onenet_data_common, "cmd", "ParamUpd");
        replaceJson(onenet_data_common, "msgID", "UART_5");     //UART参数更新，在UART中为'cmd5'命令
        replaceJson(onenet_data_common, "vol", cat1.param.voltage);
        replaceJson(onenet_data_common, "cur", cat1.param.current);
        // replaceJson(onenet_data_common, "alarm", cat1.param.alarm);  // onenet 不支持alarm
        break;

    case en_CMD_VOL_AND_CUR_ACK:
        replaceJson(onenet_data_common, "cmd", "ParamUpd");
        replaceJson(onenet_data_common, "msgID", cat1.rcv.SerRandomID);
        #ifdef VOLTAGE_ENABLE
        replaceJson(onenet_data_common, "vol", cat1.param.voltage);
        #endif
        break;
    // case en_CMD_DEV_ALARM:
    //     break;

    default:break;
    }
    liot_trace("onenet_data_common: %s", onenet_data_common);
}

/************************************************************************************
 * 名    称：onenet_update_net_signal
 * 功    能：更新数组对应的网络信号值
 * 说    明：Lierda封装的函数有问题，所以使用原厂appGetECBCInfoSync获取网络信号
************************************************************************************/
void onenet_update_net_signal(){
    liot_trace("onenet_update_net_signal start...");
    int _ret = cat1_get_net_signal();
    if(_ret == en_CAT1_GET_SIG_SUC){       // 更新网络信号参数
        liot_trace("update netSignal(pre): %s", onenet_data_common);
        replaceJson(onenet_data_common, "netSignal", cat1.netSignal);
        liot_trace("update netSignal(now): %s", onenet_data_common);
    }
    else{
        liot_trace("get net signal failed.");
    }
}

/************************************************************************************
 * 名    称：onenet_update_sub_topic_info
 * 功    能：更新订阅主题是否成功信息
 * 说    明：只有接收到平台下行数据后，才更新该信息
 * 修    改：26-01-31 这里3min替换一次，即使之前已经替换过，便于后期扩展其他字符串（因为是rev）
************************************************************************************/
void onenet_update_sub_topic_info(){
    if(cat1.server_downlink_flag){
        replaceJson(onenet_data_common, "rev", "DLIN");
    }else{
        liot_trace("========== OneNet MQTT Subscribe HeartBeat ==========");    
        e_CAT1_REG_STATUS sub_status = onenet_mqtt_sub_topic();
        if(sub_status != en_CAT1_NET_SUB_SUC){
            liot_trace("onenet mqtt subscribe failed in heartbeat!");
        }
    }
}



//-------------------------------------↑--------------------------------------
//------------------------------------上行------------------------------------
//------------------------------------下行------------------------------------
//-------------------------------------↓--------------------------------------



/*******************************************************************************
*名    称：onenet_datasend_fault
*功    能：设备异常数据发送
*入口参数：cmd-对应不同的错误命令    
*返回值：0-失败  1-成功收到平台数据
*说    明：
*修    改：
*******************************************************************************/
uint8_t onenet_datasend_fault(e_FAULT_TYPE cmd){
    uint8 cntTime = 0;

    cat1.pub_suc_flag = 0;
    switch(cmd){
    case en_FAULT_UNKNOWN:
        liot_trace("onenet_data_fault: unknown default fault.");
        replaceJson(onenet_data_fault, "data", "UnknownAll");
        replaceJson(onenet_data_fault, "msgID", "000000");
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;
    case en_FAULT_CMD:                                       
        liot_trace("onenet_data_fault: unknown command.");
        replaceJson(onenet_data_fault, "data", "UnknownCmd");
        replaceJson(onenet_data_fault, "msgID", "FFFFFF");
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;

    case en_FAULT_REMOTE_IO:
        liot_trace("onenet_data_fault: remote IO output.");
        replaceJson(onenet_data_fault, "data", "Server_Out");
        replaceJson(onenet_data_fault, "msgID", cat1.rcv.SerRandomID);
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;
   
    case en_FAULT_UART_RCV_OVERFLOW:
        liot_trace("onenet_data_fault: UART receive data overflow");
        replaceJson(onenet_data_fault, "data", "UartRcvLen");
        replaceJson(onenet_data_fault, "msgID", "FFFFFE");
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;

    case en_FAULT_UART_CMD_FREQ_OVERFLOW:
        liot_trace("onenet_data_fault: UART cmd freq overflow");
        replaceJson(onenet_data_fault, "data", "UartCmdFre");
        replaceJson(onenet_data_fault, "msgID", "FFFFFD");
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;

    case en_FAULT_UART_NO_FEEDBACK:
        liot_trace("onenet_data_fault: UART cmd no feedback");
        replaceJson(onenet_data_fault, "data", "Uart_NoAck");
        replaceJson(onenet_data_fault, "msgID", "FFFFFC");
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;

    case en_FAULT_OUT1_STATUS:
        replaceJson(onenet_data_fault, "data", "Out1StaErr");
        replaceJson(onenet_data_fault, "msgID", "StaErr");
        liot_trace("onenet_data_fault: %s", onenet_data_fault);
        break;

    default:
        liot_trace("[info] unknown fault command, could not send to AEP.");
        return 0;
    }
    

    /* 构造动态 payload：id 在 100-999 循环，value 使用 onenet_data_poweron 的 JSON */
    onenet_pub_id++;
    if (onenet_pub_id > 999) onenet_pub_id = 100;

    snprintf(onenet_payload_fault, sizeof(onenet_payload_fault),
                "{\"id\": \"%d\",\"version\": \"1.0\",\"params\": {\"Fault\": {\"value\": %s}}}",
                onenet_pub_id, onenet_data_fault);

    liot_mqtt_publish(&onenet_mqtt_cli, onenet_pub_topic, onenet_payload_fault, strlen(onenet_payload_fault), 1, 0, mqtt_requst_result_cb, NULL);
    liot_trace("publish payload length: %d, payload data: %s", strlen(onenet_payload_fault), onenet_payload_fault);


    while(cat1.pub_suc_flag && (cntTime < 80)){
        cntTime++;
        liot_rtos_task_sleep_ms(100);               // 每包数据最长等待8S
    }
    

    if(cat1.pub_suc_flag) return 1;
    return 0;
}



/*******************************************************************************
*名    称：onenet_datasend_ack
*功    能：平台下发数据，模组反馈包
*入口参数：cmd-发送不同的命令    
*返回值：0-反馈包发送失败  1-反馈包发送成功
*说    明：模组收到平台下行数据之后回复，同时回复的数据中包含自身的协议
            AT+QMTPUBEX=0,1,1,0,"$sys/6Q2HzvtYGx/865371075359413/thing/service/Downlink/invoke_reply",97
            > {"id": "8", "code": 200, "data": { "ack": "{'cmd':'RemoteAck','data':'Succ','msgID':'000000'}" }}
            OK

*修    改：
*******************************************************************************/
uint8_t onenet_datasend_ack(e_IOT_DOWN_COMMAND cmd){
    uint8 cntTime = 0;
    
    cat1.pub_suc_flag = 0;
    switch(cmd){
    case en_CMD_REMOTE_SWITCH:
        liot_trace("en_CMD_REMOTE_SWITCH ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "RemoteAck");
        iot_ack_remote_switch_replace_data_workmode();
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_REMOTE_SWITCH ack(now): %s", iot_data_ack);
        break;

    case en_CMD_POWERON_STATUS_OPEN:
        liot_trace("en_CMD_POWERON_STATUS_OPEN ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Open");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_OPEN ack(now): %s", iot_data_ack);
        break;

    case en_CMD_POWERON_STATUS_CLOS:
        liot_trace("en_CMD_POWERON_STATUS_CLOS ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Clos");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_CLOS ack(now): %s", iot_data_ack);
        break;

    case en_CMD_POWERON_STATUS_HOLD:
        liot_trace("en_CMD_POWERON_STATUS_HOLD ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Hold");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_HOLD ack(now): %s", iot_data_ack);
        break;

    case en_CMD_POWERON_STATUS_FAIL:
        liot_trace("en_CMD_POWERON_STATUS_FAIL ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "Fail");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_STATUS_FAIL ack(now): %s", iot_data_ack);
        break;

    case en_CMD_FLASH_ERROR_WRITE: 
        liot_trace("en_CMD_FLASH_ERROR_WRITE ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "MemoryAck");
        replaceJson(iot_data_ack, "data", "ErrW");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FLASH_ERROR_WRITE ack(now): %s", iot_data_ack);
        break;

    case en_CMD_FOTA_START: 
        liot_trace("en_CMD_FOTA_START ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "DiFotaAck");
        replaceJson(iot_data_ack, "data", "Stat");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FOTA_START ack(now): %s", iot_data_ack);
        break;
    case en_CMD_FOTA_ERROR_ADRESS: 
        liot_trace("en_CMD_FOTA_ERROR_ADRESS ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "DiFotaAck");
        replaceJson(iot_data_ack, "data", "ErrA");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FOTA_ERROR_ADRESS ack(now): %s", iot_data_ack);
        break;
    case en_CMD_FOTA_ERROR_LENTH: 
        liot_trace("en_CMD_FOTA_ERROR_LENTH ack(pre): %s", iot_data_ack);
        replaceJson(iot_data_ack, "cmd", "DiFotaAck");
        replaceJson(iot_data_ack, "data", "ErrL");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_FOTA_ERROR_LENTH ack(now): %s", iot_data_ack);
        break;
    
    case en_CMD_POWERON_LINK_TEST: 
        replaceJson(iot_data_ack, "cmd", "LinkT_Ack");
        replaceJson(iot_data_ack, "data", "Succ");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_POWERON_LINK_TEST ack(now): %s", iot_data_ack);
        break;
    case en_CMD_PLATFORM_FEEDBACK_FAIL:                                         // 当命令异常时，需要先反馈平台，然后在上传专属异常命令
        replaceJson(iot_data_ack, "cmd", "PlatFBAck");
        replaceJson(iot_data_ack, "data", "Fail");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_PLATFORM_FEEDBACK ack(now): %s", iot_data_ack);
        break;
    case en_CMD_PLATFORM_FEEDBACK_SUC:                                         // 部分命令反馈时，需要先反馈平台，然后在上传对应的数据
        replaceJson(iot_data_ack, "cmd", "PlatFBAck");
        replaceJson(iot_data_ack, "data", "Succ");
        replaceJson(iot_data_ack, "msgID", cat1.rcv.SerRandomID);
        liot_trace("en_CMD_PLATFORM_FEEDBACK ack(now): %s", iot_data_ack);
        break;

    default:
        liot_trace("[info] unknown command, could not ack to OneNet.");
        return 0;
    }
    

    /* 构造发布主题，使用模组的 IMEI */
    snprintf(onenet_invoke_reply_topic, sizeof(onenet_invoke_reply_topic),
            "$sys/%s/%s/thing/service/Downlink/invoke_reply",
            MQTT_CLIENT_USER, cat1.imei);

    // onenet_pub_id需要替换为平台下发的id数字，同时要测试0~11，验证id位数变化是否能够正常填充与移位
    snprintf(onenet_payload_ack, sizeof(onenet_payload_ack),
                     "{\"id\": \"%u\", \"code\": \"200\", \"data\": %s}",
                     (unsigned int)cat1.rcv.onenet_id, iot_data_ack);


    liot_mqtt_publish(&onenet_mqtt_cli, onenet_invoke_reply_topic, onenet_payload_ack, strlen(onenet_payload_ack), 1, 0, mqtt_requst_result_cb, NULL);
    
    liot_trace("publish topic length: %d, topic data: %s", strlen(onenet_invoke_reply_topic), onenet_invoke_reply_topic);
    liot_trace("publish payload length: %d, payload data: %s", strlen(onenet_payload_ack), onenet_payload_ack);


    while(cat1.pub_suc_flag && (cntTime < 80)){
        cntTime++;
        liot_rtos_task_sleep_ms(100);               // 每包数据最长等待8S
    }
    
    if(cat1.pub_suc_flag) return 1;
    return 0;
}


// ------------------------------------特殊处理------------------------------------


#define ID_MAX_VALUE 65535

/*****************************************************************************
 * @brief  从 服务器下发的数据(字符串)中，提取 "id" 字段的数字值
 * @param  payload  输入的 JSON 文本字符串，cat1.rcv.buf
 * @return uint16_t 返回提取到的 ID 值（0~65535）
 *                  如果未找到或格式错误，则返回 0
 * 
 * 功能说明：
 * 1. 支持以下格式（任意空格）：
 *      "id":"123"
 *      "id" : "123"
 *      "id": "123"
 * 2. 自动忽略空格
 * 3. 超过65535自动截断为65535
 * 4. 非数字或解析异常返回0
 ****************************************************************************/
uint16_t parse_onenet_id(const char *payload)
{
    if (!payload) return 0;

    const char *p = payload;
    const char *key = "\"id\"";
    char temp_id[16] = {0}; // 存放提取出的数字字符串

    // 1️⃣ 查找"id"关键字
    p = strstr(p, key);
    if (!p)
        return 0; // 没找到"id"

    p += strlen(key); // 跳过"id"

    // 2️⃣ 跳过空格和冒号
    while (*p && (isspace((unsigned char)*p) || *p == ':')) {
        p++;
    }

    // 3️⃣ 跳过前引号（如果有）
    if (*p == '\"') {
        p++;
    }

    // 4️⃣ 提取数字字符串
    int i = 0;
    while (*p && isdigit((unsigned char)*p) && i < (int)sizeof(temp_id) - 1) {
        temp_id[i++] = *p++;
    }
    temp_id[i] = '\0';

    if (i == 0)
        return 0; // 没有任何数字

    // 5️⃣ 转换为整数
    unsigned long val = strtoul(temp_id, NULL, 10);
    if (val > ID_MAX_VALUE)
        val = ID_MAX_VALUE;

    return (uint16_t)val;
}




/*****************************************************************************
 * @brief  从 OneNet payload 中解析 "params" 对应的 JSON 对象
 *
 * @param  payload  输入原始 JSON 字符串，例如 cat1.rcv.buf
 * @param  output   输出缓冲区，例如 cat1.onenet_buf
 *
 * @note   输出字符串长度不会超过 RCV_BUF_ONENET_SIZE_256-1
 *         若解析失败，输出为空字符串
 * 
 * 输入字符串：{"id":"15","version":"1.0","params":{"cmd":"RmtSwich","data":"0001","msgID":"36E2D5","workmode":"Swt"}} 
 * 输出结果：{"cmd":"RmtSwich","data":"0001","msgID":"36E2D5","workmode":"Swt"}
 *****************************************************************************/
void parse_onenet_params(const char *payload, char *output)
{
    const char *key = "\"params\":";
    const char *start = NULL;
    const char *p = NULL;
    int brace_count = 0;

    // 1️⃣ 查找 "params": 关键字
    start = strstr(payload, key);
    if (!start) {
        output[0] = '\0';
        return;
    }

    // 2️⃣ 移动指针到 key 后面
    start += strlen(key);

    // 3️⃣ 跳过空格，确保定位到 '{'
    while (*start == ' ' || *start == '\t') start++;

    if (*start != '{') {
        output[0] = '\0';
        return;
    }

    // 4️⃣ 使用 brace_count 匹配大括号，提取完整 JSON 对象
    p = start;
    brace_count = 0;
    while (*p != '\0') {
        if (*p == '{') brace_count++;
        else if (*p == '}') brace_count--;

        p++;

        if (brace_count == 0) break;
    }

    // 5️⃣ 如果大括号不匹配，返回空
    if (brace_count != 0) {
        output[0] = '\0';
        return;
    }

    // 6️⃣ 计算长度，并限制在缓冲区大小内
    size_t len = p - start;
    if (len >= RCV_BUF_ONENET_SIZE_256) {
        len = RCV_BUF_ONENET_SIZE_256 - 1;
    }

    // 7️⃣ 复制内容到输出缓冲区，并添加字符串结束符
    memcpy(output, start, len);
    output[len] = '\0';
}

