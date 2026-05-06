// #include "slpman.h"
// #include "liot_os.h"
// #include "liot_power.h"
// #include "liot_type.h"
// #include "lierda_app_main.h"

#include "hal_project.h"


liot_timer_t sys_timer_1s = NULL;


void sys_timer1s_callback(void *argv){
    // 更新所有计数器
    cat1.heart_cnt++;
    cat1.disconnect_1min_cnt++;
    cat1.net_led_cnt++;
    cat1.init_cnt++;
    cat1reboot.disconnect_cnt++;                    // 超过 1hour/24hour 重启
    cat1reboot.reboot_24hour_cnt++;           

    uart2.cmd3_sec_cnt++;                           // CMD3 状态帧发送计数器，达到 2s（count>=2）时发送一次，并清 0
    if(uart2.cmd4_platform_rcv_cnt++ >= 65535){     // 10~65535 之间循环，平台下发命令时会清 0，UART 收到数据后判断是否 2S 内收到平台下发的命令，如果是则不响应并直接反馈，避免平台下发与本地 UART 冲突问题
        uart2.cmd4_platform_rcv_cnt = TIME_PLATFORM_CMD_PROTECT_2S;
    }
    if(uart2.cmd7_platform_rcv_cnt++ >= 65535){     // 10~65535 之间循环，平台下发命令时会清 0，UART 收到数据后判断是否 2S 内收到平台下发的电机控制命令，如果是则不响应并直接反馈，避免平台下发与本地 UART 冲突问题
        uart2.cmd7_platform_rcv_cnt = TIME_PLATFORM_CMD_PROTECT_2S;
    }

    /* 【新增】电压电流更新计时器：1s 自增，3min(180s) 后清 0 计数器 */
    if (uart2.cmd_vol_cur_timer_cnt < 180) {
        uart2.cmd_vol_cur_timer_cnt++;
    } else {
        /* 达到 3 分钟，清 0 计数器和计时器 */
        uart2.cmd_vol_cur_update_cnt = 0;
        uart2.cmd_vol_cur_timer_cnt = 0;
    }

    // 处理心跳逻辑：如果初始化成功且心跳计数达到阈值，通知平台任务发送心跳
    if(cat1.initSucFlag && cat1.heart_cnt >= TIME_CAT1_HEARTBEAT_3MIN){
        comm_msg_t msg;
        msg.source = TRIGGER_SOURCE_TIMER;  // 假设定义了 TRIGGER_SOURCE_TIMER，表示定时器触发
        msg.type = MSG_TYPE_TIMER_TRIGGER_HEARTBEAT;      // 新增：消息类型为心跳

        // 非阻塞入队（若队列已满则返回失败），在任务/回调上下文可使用
        liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0);
    }

    // 处理离线重连逻辑：如果离线且断连计数超过阈值，通知平台任务尝试重连
    if(!cat1.initSucFlag && cat1.disconnect_1min_cnt > TIME_CAT1_RECONNECT_1MIN){
        comm_msg_t msg;
        msg.source = TRIGGER_SOURCE_TIMER;  // 假设定义了 TRIGGER_SOURCE_TIMER
        msg.type = MSG_TYPE_TIMER_TRIGGER_RECONNECT;      // 新增：消息类型为重连

        // 非阻塞入队（若队列已满则返回失败）
        liot_rtos_queue_release(platform_queue, sizeof(msg), (uint8_t*)&msg, 0);
    }
}


/*******************************************************************************
 * 名    称：rtos_timer_init
 * 功    能: 系统定时器初始化(可以创建任意多个)
 * 说    明: 
 *******************************************************************************/
void rtos_timer_init(){
    liot_rtos_timer_create(&sys_timer_1s, 1, sys_timer1s_callback, NULL);
    liot_rtos_timer_start(sys_timer_1s, 1000);                             // 定时器周期1S

    liot_trace("rtos_timer_init() complete");
}
