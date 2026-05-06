/***************************************************************************
 * @File Name: customer_app.c
 * @Author : ymx
 * @Version : 6.0
 * @Creat Date : 2026-02-16
 * @copyright Copyright (c) 2026 Lierda Science & Technology Group Co., Ltd.

 * 说    明：不同该版本修改位置
 *          0. 区分hal_project.h中模块型号NT26KCNB00NNC与NT21KCNA
 *          1. 修改modle_config.cfg中的clientVersion，后期可通过AT+CGMR读取版本
 *          2. 修改modle_config.cfg中的模组型号，匹配区分hal_project.h中模块型号
 *          3. customer_app.c中liot_trace打印信息
 *          4. drv_onenet.c和drv_aep.c中aep_data_send_poweron[]信息
 *              "product"
 *              "ver"
 *              "workmode"    
 * 
 * 如果切换SDK：bsp_config.h需设置"AT"、禁止"RNDIS"
 *            mv_nvm_config.c中设置上电后默认为CFUN0
 *            PLAT\lierda_config\open_config.inc THIRDPARTY_MBEDTLS_ENABLE = n，节省80k空间
***************************************************************************/
#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "cmidev.h"
#include "ps_dev_if.h"

#include "hal_gpio.h"
#include "hal_project.h"


void app_print_build_config();
void sysPwrOn_ParamInit();
void sysPwrOn_ReasonGet();
void sysPwrOn_SetQualityFirst();

/*******************************************************************************
* 名    称：liot_main
* 功    能：系统入口函数，完成系统初始化和主任务创建
* 说    明：
*   - 优化初始化流程，结构更清晰，便于维护和扩展。
*   - 各功能模块初始化采用分步调用，便于定位和调试。
*   - 支持多产品类型（开关/电机），通过宏自动切换任务创建。
*   - 增加详细注释，便于后续开发和维护。
* 修    改：
*******************************************************************************/
void liot_main(void *argument) {
    sysPwrOn_SetQualityFirst();                 // CFUN0，必须上电后立即执行
    sysPwrOn_ParamInit();                       // 系统参数(变量)上电初始化
    sysPwrOn_ReasonGet();                       // 获取上电原因

    liot_trace("HS NT26K0B1 Switch UART 6.3.1");
    app_print_build_config();                   // 打印编译配置信息

    rtos_timer_init();                          // 定时器周期1S/次 
    net_led_init();
    gpio_out_init();                            // GPIO输出IO初始化（开关产品或电机产品）
    FlashRead_PowerOn();                        // 先初始化IO口，才能读取Flash
    #if UART_COMM_ENABLE
    liot_create_uart_task();                    // UART通信线程
    liot_rtos_task_sleep_ms(1500);              // 确保UART任务先于通信任务执行，即先发送PWRON帧，在发送其他状态帧(实测1s不够，这里延时1.5s)
    #endif

    #if KEY_SWITCH_ENABLE
    liot_create_key_task();                     // 按键通信线程
    #endif

    liot_rtos_queue_create(&platform_queue, sizeof(comm_msg_t), 5); // 创建队列：item_size = sizeof(comm_msg_t)，depth = 5
    #if CT_AEP
    liot_create_aep_task();                     // AEP通信线程，10mS周期
    #elif CMCC_ONENET
    liot_create_onenet_task();                  // OneNet通信线程，10mS周期
    #endif


    // 打印系统内存信息
    liot_trace("minimum / free / totle heap size: %dKB, %d KB, %d KB", xPortGetMinimumEverFreeHeapSizeEc() >> 10, liot_xPortGetFreeHeapSize() >> 10, liot_xPortGetTotalHeapSize() >> 10);
    liot_rtos_task_sleep_s(10);                 // 延时，确保初始化完成

    // 删除当前任务，释放资源
    liot_osi_errcode_e ret = liot_rtos_task_delete(NULL);
    if (ret != LIOT_OSI_SUCCESS) {
        liot_trace("dev task deleted failed");
    }
}


/*******************************************************************************
*名    称：sysPwrOn_ParamInit()
*功    能：系统上电后，初始变量参数初始化
*说    明：
*修    改：
*******************************************************************************/
void sysPwrOn_ParamInit(){
    memset(cat1.pwrReason, 0, sizeof(cat1.pwrReason));  // 赋值之前，先清0
    strcpy(cat1.pwrReason, "UKN");

    for (int i = 0; i < 4; i++) {
        g_io.out[i].pre_status = enIO_OUT_OFF;          // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
        g_io.out[i].now_status = enIO_OUT_OFF;          // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
    }

    cat1.init_cnt = 0;
    cat1.server_downlink_flag = 0;                      // 服务器下发数据标志位，用于判断是否topic订阅成功 26-01-31
    cat1.pwr_send_flag = 0;
    cat1.rcv.flag = 0;
    cat1.rcv.poweron_info_flag_get = 0;                 // 是否需要获取开机数据，底层不好更改，所以使用标志位
    cat1.ceregCfunFlag = 0;
    cat1.disconnect_1min_cnt = 0;
    cat1.disconnect_30min_cnt = 0;
    cat1reboot.disconnect_cnt = 0;                
    cat1reboot.reboot_24hour_cnt = 0;                  
    cat1.cereg_access_time = enLONG_120S;
    cat1.param.UpdateFlag = 0;
    cat1.param.voltage_sum = 0;                         // 电压总和，用于计算平均电压
    cat1.param.voltage_count = 0;                       // 电压计数，用于计算平均电压
    cat1.param.dev_fault = DEV_FAULT_NONE;              // 设备故障状态，默认"0"，注意这里是UART发送，非平台侧协议

    strcpy(cat1.vbat, "3.8V");
    strcpy(cat1.temp, "+25C");
    strcpy(cat1.param.voltage, "000.0");
    strcpy(cat1.param.current, "000.0");
    strcpy(cat1.param.alarm, "NON");
    strcpy(cat1.memState, "NULL");

    uart2.messageIDCounter = 0;
    uart2.cmd3_sec_cnt = 0;
    uart2.cmd4_platform_rcv_cnt = TIME_PLATFORM_CMD_PROTECT_2S;
    uart2.cmd7_platform_rcv_cnt = TIME_PLATFORM_CMD_PROTECT_2S;
    uart2.cmd_vol_cur_update_cnt = 0;
    uart2.cmd_vol_cur_timer_cnt = 0;  // 计时器初始化为 0
    uart2.isRcvIoSwitch = 0;
    uart2.isIoSwitchFeedback = 0;
    
    /* 初始化电机状态 */
    motor.currentState = MODE_IDLE_HOLD;  // 初始化为停止状态
    motor.preState = MODE_IDLE_HOLD;      // 上一个状态也初始化为停止状态

    // uart2.isInitialized = 0;
    // uart2.heartbeatCounter = 0;
    // uart2.shouldSendHeartbeat = 0;
    // uart2.cycle_10min_count = 0;
    // uart2.cmd_interaction_count_10min = 0;
    // uart2.faultType = UART_FAULT_NONE;
    // uart2.paramGetFlag = 0;                     // 平台主动获取串口参数获取标志位，默认 0
    // uart2.paramGetSuccess = 0;                  // 平台主动获取串口参数获取成功标志位，默认 0，该标志位用于逻辑判断
    // strcpy(uart2.preAlarmType, "NON");          // 记录上次报警类型 (最多 3 个)，用于判断是否重复报警，默认"NON"
    // appEvent = APP_EVENT_NONE;

    strcpy(cat1.msgIDStr, "DEFULT");
}

/*******************************************************************************
*名    称：sysPwrOn_ReasonGet()
*功    能：系统上电后，默认功能初始化
*说    明：
    typedef enum
    {
        LIOT_PWRUP_UNKNOWN,    // unknown reason             UKN
        LIOT_PWRUP_PWRKEY,     // power up by power key      PWR
        LIOT_PWRUP_PIN_RESET,  // power up by pin reset      RST
        LIOT_PWRUP_ALARM,      // power up by alarm          ALM
        LIOT_PWRUP_CHARGE,     // power up by charge in      CHA
        LIOT_PWRUP_WDG,        // power up by watchdog       WDG
        LIOT_PWRUP_PSM_WAKEUP, // power up from PSM wakeup   WAK
        LIOT_PWRUP_PANIC       // power up by panic reset    PAN
    } liot_pwrup_reason_e;
*修    改：26-02-15 上电原因打印由数字改为字符串，便于理解和调试
*******************************************************************************/
void sysPwrOn_ReasonGet(){
    UINT8 pwr_reason = 0;
    liot_power_errcode_e get_pwr_result;

    get_pwr_result = liot_get_powerup_reason(&pwr_reason); 
    if(get_pwr_result != LIOT_POWER_RESET_SUCCESS){
        liot_trace("[info] get poweron reason fail!!!");
        strcpy(cat1.pwrReason, "UKN");
        return;
    }
    
    const char *pwr_reason_str = "UKN";
    static const char *pwr_reason_map[] = {
        "UKN", /* LIOT_PWRUP_UNKNOWN */
        "KEY", /* LIOT_PWRUP_PWRKEY  */
        "RST", /* LIOT_PWRUP_PIN_RESET */
        "ALM", /* LIOT_PWRUP_ALARM */
        "CHA", /* LIOT_PWRUP_CHARGE */
        "WDT", /* LIOT_PWRUP_WDG */
        "PSM", /* LIOT_PWRUP_PSM_WAKEUP */
        "PNC", /* LIOT_PWRUP_PANIC */
    };
    if (pwr_reason < (sizeof(pwr_reason_map) / sizeof(pwr_reason_map[0]))) {
        pwr_reason_str = pwr_reason_map[pwr_reason];
    }
    liot_trace("    poweron reason is: %s", pwr_reason_str);

    switch (pwr_reason){
    case LIOT_PWRUP_PWRKEY:
        strcpy(cat1.pwrReason, "KEY");
        break;
    case LIOT_PWRUP_PIN_RESET:
        strcpy(cat1.pwrReason, "RST");
        break;
    case LIOT_PWRUP_ALARM:
        strcpy(cat1.pwrReason, "ALM");
        break;
    case LIOT_PWRUP_CHARGE:
        strcpy(cat1.pwrReason, "CHA");
        break;
    case LIOT_PWRUP_WDG:
        strcpy(cat1.pwrReason, "WDT");
        break;
    case LIOT_PWRUP_PSM_WAKEUP:
        strcpy(cat1.pwrReason, "PSM");
        break; 
    case LIOT_PWRUP_PANIC:
        strcpy(cat1.pwrReason, "PNC");
        break; 
    default:
        strcpy(cat1.pwrReason, "UKN");
        break;
    }
}

/*******************************************************************************
*名    称：sysPwrOn_SetQualityFirst()
*功    能：设置网络信号优先，如果邻区的信号较低不进行切换
*说    明：
*修    改：25-02-27 增加liot_gpio_set_voltage(LIOT_VOL_2_80V);
*******************************************************************************/
void sysPwrOn_SetQualityFirst(){
    CmiRcCode ret;
    CmiDevSetExtCfgReq setExtCfgReq;            // 64 bytes

    liot_set_sleep_depth(LIOT_SLP_IDLE_STATE);  // 设置最大休眠深度

    liot_dev_set_modem_fun(0, 0, 0);            // AT+CFUN=0//保持AION电源打开
    liot_aon_power_on();                	    // 开启AON IO
    liot_gpio_set_voltage(LIOT_VOL_2_80V);      // 设置AION电压2.8V


    liot_rtos_task_sleep_s(1); 
    liot_trace("set CFUN0 end");

    //查询并判断相关参数是否支持
    CmiDevGetExtCfgCnf getExtCfgCnf = {0};      // 36 bytes
    ret = devGetExCfgParaSync(&getExtCfgCnf);
    if (ret != CME_SUCC){
        liot_trace("QualityFirst not support");
        return;
    }

    //设置信号优先
    liot_trace("QualityFirst set start!");
    memset(&setExtCfgReq, 0x00, sizeof(CmiDevSetExtCfgReq));
    setExtCfgReq.qualityFirstPresent = TRUE;
    setExtCfgReq.qualityFirst = 2;
    setExtCfgReq.reselToWeakNcellOptPresent = TRUE;
    setExtCfgReq.reselToWeakNcellOpt = 0;
    devSetExCfgParaSync(&setExtCfgReq);

    liot_dev_set_modem_fun(1, 0, 0);            // AT+CFUN=1
    liot_trace("QualityFirst set complete!");
}


