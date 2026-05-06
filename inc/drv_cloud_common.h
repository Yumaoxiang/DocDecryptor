#ifndef __DRV_CLOUD_COMMON_H__
#define __DRV_CLOUD_COMMON_H__

#include "liot_dev.h"
#include "liot_nw.h"
#include "liot_mqtt_client.h"


#define IMEI_SIZE                   15
#define ICCID_SIZE_20               20
#define ALARM_STR_LEN               3

#define TIME_CAT1_INIT_2MIN         120     //每次中断 1S
#define TIME_CAT1_HEARTBEAT_3MIN    180
#define TIME_CAT1_RECONNECT_1MIN    60
#define TIME_CAT1_RECONNECT_30MIN   30
#define TIME_CAT1_RECONNECT_12HOUR  720
#define TIME_CAT1_DISCONNECT_1HOUR  3600
#define TIME_CAT1_DISCONNECT_24HOUR 86400

#define RCV_KEY_SIZE_10             10
#define RCV_VALUE_SIZE_10           10
#define RCV_BUF_ONENET_SIZE_256     256 
#define RCV_BUF_SIZE_512            512     // 不在支持离线定时，所以这里下行长度修改为 512


typedef enum {
    enSHOUT_15S,                     
    enLONG_120S,                   
} e_CEREG_TIME;

typedef enum {
    en_STATUS_OFF=0,                    //LED 引脚输出状态，
    en_STATUS_ON=1,                     //LED 引脚输出状态
} e_NET_LED_STATUS;

typedef enum {
    en_CMD_NULL=0,                      //默认值
    en_CMD_POWERON=1,                   //设备开机
    en_CMD_HEART_BEAT=2,                //心跳包
    en_CMD_LOCAL_SWITCH=3,              //本地按键开关
    en_CMD_UART_SWITCH_IO=4,            //UART 开关状态变化
    en_CMD_UART_SWITCH_PARAM_UPDATE=5,  //UART 参数更新
    en_CMD_UART_MOTOR_IO=6,             //UART 电机状态变化
    en_CMD_UART_MOTOR_PARAM_UPDATE=7,   //UART 电机参数更新
    en_CMD_SYS_FAULT=10,                 //系统异常
    // en_CMD_VOL_AND_CUR_UPDATE=20,       //电压电流更新
    en_CMD_VOL_AND_CUR_ACK=21,      //电压电流更新 ack
    en_CMD_DEV_ALARM=22,            //设备报警
} e_COMMON_UP_COMMAND;

typedef enum {
    enCAT1_MODULE_FW,                   // 读取模组型号及版本号
    enCAT1_IMEI,                        // 读取 IMEI
    enCAT1_ICCID,                       // 读取 ICCID
    enCAT1_NET_ACCESS,                  // 查询是否网络已经附着
    enCAT1_CLOUD_REG,                   // 云平台注册 
    // enNB_CTM2MSETPM,            //连接电信平台
    // enNB_CTM2MREG,               //注册 IOT 平台
	// enNB_CTM2MDEREG,             //去注册 IOT 平台
	// enNB_SENDDATA,                 //发送数据
    // enNB_REG_SUC,               //注册成功
	enCAT1_INIT_SUC,               //成功初始化
    enCAT1_INIT_FAIL,             //初始化失败
} cat1_init_status_t;

typedef enum {
    en_CAT1_LED_NULL=0,             // 默认值
    en_CAT1_LED_MODULE_FW=1,        // 读模组型号，闪烁周期 4S：2S 亮/2S 灭
    en_CAT1_LED_IMEI=2,             // 读卡之前所有操作，闪烁周期 2S：1S 亮/1S 灭
    en_CAT1_LED_SIM_CARD=3,         // 读卡，闪烁周期 2S：10mS 脉冲 * 1
    en_CAT1_LED_SIM_MATCH = 4,      // 校验 SIM 卡是否与运营商匹配，闪烁周期 2S：10mS 脉冲 * 2
    en_CAT1_LED_NET=5,              // 网络注册，闪烁方式 1S：10mS 脉冲 * 1
    en_CAT1_LED_CLOUD_REG=6,        // 平台注册，闪烁方式 1S：10mS 脉冲 * 2 
} e_CAT1_REG_AEP_LED_MODE;          // Cat.1 注册过程中 led 闪烁方式


typedef enum {
    en_CAT1_NULL=0,                 // 默认值
    en_CAT1_SUC=1,                  // 交互成功，通用返回值
    en_CAT1_FAIL=2,                 // 交互失败，通用返回值
    
    en_CAT1_GET_MODULEFW_SUC=3,     // 获取模组型号成功
    en_CAT1_GET_IMEI_SUC=4,         // 获取 IMEI 成功
    en_CAT1_GET_ICCID_SUC=5,        // 获取 ICCID 成功
    en_CAT1_NET_REG_SUC=6,          // 平台注册成功
    en_CAT1_NET_SUB_SUC=7,          // 平台订阅成功
    en_CAT1_GET_SIG_SUC=8,          // 获取网络信号成功

    en_CAT1_CFUN_SUC=10,
} e_CAT1_REG_STATUS;                // Cat.1 注册过程中交互结果返回值

typedef enum {
    en_FAULT_UNKNOWN=0,
    en_FAULT_CMD=1,
    en_FAULT_REMOTE_IO=2,
    en_FAULT_TIMER_IO=3,
    en_FAULT_TIMER_THE=4,
    en_FAULT_UART_RCV_OVERFLOW=5,
    en_FAULT_UART_CMD_FREQ_OVERFLOW=6,
    en_FAULT_UART_NO_FEEDBACK=7,
    en_FAULT_OUT1_STATUS=8,                 //致命错误调试，排查 OUT1 的开关变量是否被异常篡改 25-06-15

    en_FAULT_MOTOR_SER_STATUS_DISMATCH=10,
    en_FAULT_MOTOR_SER_STATUS_LOGIC=11,     //逻辑状态错误，如正转状态下发反转命令
    en_FAULT_MOTOR_SER_STATUS_REPEAT=12,    //逻辑状态错误，如正转状态下发正传命令
    en_FAULT_MOTOR_SER_STATUS_UNKNOWN=13,   //未知逻辑状态 (非 stop/forw/revr)
} e_FAULT_TYPE;

typedef enum {
    en_CMD_REMOTE_SWITCH=2,
    en_CMD_POWERON_STATUS_CLOS=3,
    en_CMD_POWERON_STATUS_OPEN=4,
    en_CMD_POWERON_STATUS_HOLD=5,
    en_CMD_POWERON_STATUS_FAIL=6,
    en_CMD_TIMER_SUCC=7,
    en_CMD_TIMER_FAIL=8,
    en_CMD_TIMER_REMOTE_STOP=9,
    en_CMD_FOTA_START=10,
    en_CMD_FOTA_ERROR_ADRESS=11,
    en_CMD_FOTA_ERROR_LENTH=12,
    en_CMD_FLASH_ERROR_WRITE=13,
    en_CMD_SET_LIMIT_SUCC=14,
    en_CMD_SET_LIMIT_FAIL=15,
    en_CMD_POWERON_LINK_TEST=16,

    en_CMD_PLATFORM_FEEDBACK_SUC=17, // 通用类平台反馈命令
    en_CMD_PLATFORM_FEEDBACK_FAIL=18, // 通用类平台反馈命令
} e_IOT_DOWN_COMMAND;

typedef enum {  // 运营商枚举，SIM 卡匹配不同运营商
    OP_UNKNOWN = 0,
    OP_CMCC,    // 中国移动
    OP_CUCC,    // 中国联通
    OP_CTCC     // 中国电信
} operator_t;

/* 电机状态枚举 */
typedef enum {
    MODE_IDLE_HOLD = 0,     // 联动停止
    MODE_IDLE_JOG,          // 点动停止
    MODE_FORWARD_HOLD,      // 联动正转/保持正转
    MODE_FORWARD_JOG,       // 点动正转
    MODE_REVERSE_HOLD,      // 联动反转/保持反转
    MODE_REVERSE_JOG,       // 点动反转
} motor_state_t;

/* 电机结构体 */
typedef struct {
    motor_state_t currentState;  // 当前电机状态
    motor_state_t preState;      // 上一个电机状态（用于判断状态是否发生变化）
} motor_info_t;
extern motor_info_t motor;

typedef enum{   // 平台下行数据，设备确认结果
    en_PLATFORM_CMD_CONFIRM_SUC = 0,                // 平台命令确认成功
    en_PLATFORM_CMD_CONFIRM_FAIL = 1,               // 平台命令确认失败
    en_PLATFORM_CMD_CONFIRM_FAIL_REMOTE_IO = 2,     // 平台命令确认失败，远程 IO 异常
    en_PLATFORM_CMD_CONFIRM_FAIL_MEM_STATE = 3,     // 平台命令确认失败，状态记忆异常
    en_PLATFORM_CMD_CONFIRM_FAIL_UART_FEEDBACK = 4, // 平台命令确认失败，UART 没有反馈 ACK
} e_PLATFORM_CMD_CONFIRM_RESULT;


typedef struct _cat1_rcv {
    uint16 len; 
    char buf[RCV_BUF_SIZE_512];
    char cmd[RCV_KEY_SIZE_10];
    char data[RCV_VALUE_SIZE_10];
    char SerRandomID[6+1];          //服务器下发的随机数 3Bytes，使用 extracJson 函数最后 1 位会赋值'\0'
    char workmode[4];               // Jog/Hld/Swt
    uint8 flag;
    uint8 poweron_info_flag_get;    // 是否需要获取开机数据，底层不好更改，所以使用标志位

    uint16 onenet_id;
    char onenet_buf[RCV_BUF_ONENET_SIZE_256];
} cat1_rcv;

#define SET_LIMIT_LEN 32
typedef struct _prduct_param {
    uint8_t UpdateFlag;     // 参数更新标志位，0-未更新，1-已更
    char voltage[6];        // 整机电压检测数值，固定 5 位，如 220.0
    char current[6];        // 整机电流检测数值，固定 5 位，如 030.8
    char alarm[4];          // 报警信息，固定 4 位，如 "NON"
    char dev_fault;         // 设备故障状态，固定 1 位，如 "0"，注意这里是 UART 发送，非平台侧协议
    // 电压电流限值数据，格式如：{"OV:250.0|UV:180.0|OC:FFFFF"}，长度不超过 32 字节
    // OV:过压，UV:欠压，OC:过流，FFFFF
    char limitData[SET_LIMIT_LEN];     // 电压电流限值数据，格式如：{"OV:250.0|UV:180.0|OC:FFFFF"}，长度不超过 32 字节
    char underVoltage[6];   // 整机欠压检测数值，固定 4 位，如 180.0
    char overVoltage[6];    // 整机过压检测数值，固定 4 位，如 250.0
    char overCurrent[6];    // 整机过流检测数值，固定 4 位，如 030.8

    int voltage_sum;        // 电压总和，用于计算平均电压
    int voltage_count;      // 电压计数，用于计算平均电压
} prduct_param_t;

typedef struct _oem_info {
    cat1_init_status_t reg_status;
    
    char pwrReason          [3+1];  // 模组上电原因
    char imei               [IMEI_SIZE + 1];
    char iccid              [ICCID_SIZE_20+1];
    char netSignal          [8+1];
    char memState           [4+1];
    
    uint8 net_led_cnt;
    uint16 init_cnt;                // 相当于以前的 globalCnt
    uint16 heart_cnt;               // 心跳包计数
    uint16 disconnect_1min_cnt;     // 设备断连计数，1min 快速搜网 5S
    uint16 disconnect_30min_cnt;    // 设备断连计数，30min 慢速搜网 2min
    e_CEREG_TIME cereg_access_time; //  搜网时间

    uint8 server_downlink_flag;     // 服务器下发数据标志位，用于判断是否 topic 订阅成功 26-01-31
    uint8 pwr_send_flag;            // 如果先开机，在注册，会错过开机数据，通过判断决定是否重发
    volatile uint8 reg_flag;        // 平台注册标志位，属于中间件
    volatile uint8 sub_flag;        // 平台订阅标志位，属于中间件
    volatile uint8 pub_suc_flag;    // 发送数据是否成功标志位
    volatile uint8 initSucFlag;     // 初始化成功标志位，全局其他位置检查该标志位判断
    uint8 deinitSucFlag;            // 去初始化，通信异常时断开连接判断标志位
    uint8 ceregCfunFlag;            // 短周期时，是否 CFUN0/CFUN1 判断标志位

    cat1_rcv rcv;                   // 接收相关标志位

    char temp[5];                   // 模组温度检测值，固定 5 位，如+45℃，-15℃
    char vbat[5];                   // 模组电压检测数值，固定 4 位，如 3.8V

    char msgIDStr[6+1];             // msgID 字符串，用于记录不同触发源的 msgID

    prduct_param_t param;           // 产品参数，电压等
    uint8 alarm_flag;               // 报警标志位，0-未报警，1-已报警
} cat1_info_t;
extern cat1_info_t cat1;

typedef struct _cat1_reboot {
    uint32 disconnect_cnt;        // 设备断连计数，如果超过 1hour 则重启
    uint32 reboot_24hour_cnt;     // 24 小时内只允许重启 1 次
} cat1_reboot_t;
extern cat1_reboot_t cat1reboot;



typedef enum {                      // 触发源枚举，区分不同触发源以便于队列处理
    TRIGGER_SOURCE_UART = 0,
    TRIGGER_SOURCE_KEY  = 1,
    TRIGGER_SOURCE_TIMER = 2,
    TRIGGER_SOURCE_PLATFORM = 3,    // 平台下发（通用），移动 or 电信
} trigger_source_t;

typedef enum {                                      // 触发源枚举，区分不同触发源以便于队列处理
    MSG_TYPE_TIMER_TRIGGER_HEARTBEAT = 0,           // 心跳消息
    MSG_TYPE_TIMER_TRIGGER_RECONNECT = 1,           // 重连消息
    MSG_TYPE_UART_TRIGGER_SWITCH_STATUS_CHANGE = 2, // IO 状态变化消息
    MSG_TYPE_UART_TRIGGER_SWITCH_PARAM_UPDATE = 3,  // UART 参数更新消息
    MSG_TYPE_UART_TRIGGER_MOTOR_STATUS_CHANGE = 4,  // UART 电机状态变化消息
    MSG_TYPE_UART_TRIGGER_MOTOR_PARAM_UPDATE = 5,   // UART 电机参数更新消息
    MSG_TYPE_PLATFORM_TRIGGER_DOWNLINK = 6,         // 平台下发消息
    MSG_TYPE_UART_RCV_TRIGGER = 7,                  // UART 接收消息，触发源为 UART
    MSG_TYPE_KEY_TRIGGER_IO_STATUS_CHANGE = 8,      // 按键 IO 状态变化消息
} trigger_type_t;

// 消息项
typedef struct {
    trigger_source_t source;        // 触发来源：UART/KEY/TIMER/PLATFORM
    trigger_type_t type;           // 触发类型：HEARTBEAT/RECONNECT/IO_STATUS_CHANGE/PARAM_UPDATE/DOWNLINK/UART_RCV
} comm_msg_t;

extern liot_queue_t platform_queue; // 名称含义：平台相关消息队列（包含 AEP/OneNet/其他来源）




void mqtt_connect_result_cb(liot_mqtt_client_t *client, void *arg, int status);
void mqtt_state_exception_cb(liot_mqtt_client_t *client);
void mqtt_requst_result_cb(liot_mqtt_client_t *client, void *arg, int err);
void mqtt_disconnect_result_cb(liot_mqtt_client_t *client, void *arg, int err);
void mqtt_inpub_data_cb(liot_mqtt_client_t *client, void *arg, int pkt_id, const char *topic, const unsigned char *payload, unsigned short payload_len);


e_CAT1_REG_STATUS cat1_get_modulefirmware_n_times(uint8_t ntimes);
e_CAT1_REG_STATUS cat1_get_modulefirmware(void);

e_CAT1_REG_STATUS cat1_get_imei_n_times_with_timeout(uint8 ntimes);
e_CAT1_REG_STATUS cat1_get_imei_n_times(uint8 ntimes);
e_CAT1_REG_STATUS cat1_get_imei();
uint8_t IMEIScopeCheck();

e_CAT1_REG_STATUS cat1_get_iccid_n_times_with_timeout(uint8 ntimes);
e_CAT1_REG_STATUS cat1_get_iccid_n_times(uint8 ntimes);
e_CAT1_REG_STATUS cat1_get_iccid();
operator_t get_iccid_operator(const char *iccid);
void ICCID_OperatorCheck();

e_CAT1_REG_STATUS cat1_net_acess_n_times_with_timeout(uint8 ntimes);
e_CAT1_REG_STATUS cat1_net_acess_n_times(uint8 ntimes);

e_CAT1_REG_STATUS cat1_get_net_signal();
void cat1_reg_led_check(e_CAT1_REG_AEP_LED_MODE mode);
e_CAT1_REG_STATUS cat1_set_cfun_ntimes(liot_dev_cfun_e mode, uint8 ntimes, cat1_init_status_t status);



uint8 cat1_init(e_COMMON_UP_COMMAND initCmd);
void cat1_init_register();

uint8_t datasend_n_times_with_reconnect(e_COMMON_UP_COMMAND cmd, uint8 ntimes);
uint8_t datasend_n_times_with_initcheck(e_COMMON_UP_COMMAND cmd, uint8 ntimes);
uint8_t datasend_n_times(e_COMMON_UP_COMMAND cmd, uint8 ntimes);

void datasend_fillproto_common_replace_output(e_COMMON_UP_COMMAND com);
void replaceJson(char *str, const char *srckey, const char *desvalue);
void extractJson(char *str, char *key, char *value, uint16 valuelen);

void iot_datasend_fault(e_FAULT_TYPE cmd);


void handle_key_trigger(comm_msg_t *msg);

void handle_timer_trigger(comm_msg_t *msg);

// Helper: stringify trigger enums for readable logging
const char *trigger_source_to_str(trigger_source_t s);
const char *trigger_type_to_str(trigger_type_t t);
void handle_heartbeat_message(comm_msg_t *msg);
void handle_reconnect_message(comm_msg_t *msg);
void SysHeartBeat_Reboot();


void handle_uart_trigger(comm_msg_t *msg);


void handle_platform_trigger(comm_msg_t *msg);




// Helper: stringify trigger enums for readable logging (shared)
const char *trigger_source_to_str(trigger_source_t s);
const char *trigger_type_to_str(trigger_type_t t);
void log_received_message(const comm_msg_t *msg);  // 记录接收到的消息信息

void SeverDataRead();
void SeverDataRead_CmdExcute(char *serCmd);
void SeverDataRead_CmdExcute_Extract_msgID();
void SeverDataRead_CmdExcute_Extract_msgID_and_data();

void SeverDataRead_CmdExcute_Fota_Ack();
bool fota_containsPar(const char *str);
void iot_datasend_ack(e_IOT_DOWN_COMMAND cmd);
void iot_ack_remote_switch_replace_data_workmode();
void SeverDataRead_CmdExcute_LinkTest();
void ServerDataRead_CmdExcute_PlatformAck(e_IOT_DOWN_COMMAND cmd);


void SeverDataRead_CmdExcute_RmtSwich();
e_PLATFORM_CMD_CONFIRM_RESULT RmtSwich_ConfirmIOState();
void RmtSwich_ExcuteIo_and_UpdateIoState(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result);
void RmtSwich_Feedback_to_Platform(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result);
// Motor 产品相关函数声明
e_PLATFORM_CMD_CONFIRM_RESULT LocSwich_ConfirmMotorState();
void LocSwich_Excute_and_UpdateMotorState(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result);
void LocSwich_UpdateMotorState_and_Feedback(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result);
void handle_successful_confirmation();
void handle_failed_confirmation();

void SeverDataRead_CmdExcute_MemState();
void SeverDataRead_CmdExcute_MemState_Cat1();
void SeverDataRead_CmdExcute_MemState_Mcu();
e_PLATFORM_CMD_CONFIRM_RESULT MemState_ConfirmUART();
void MemState_Feedback(e_PLATFORM_CMD_CONFIRM_RESULT confirm_result);

#endif