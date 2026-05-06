#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include "lierda_app_main.h"
#include "liot_os.h"
#include "cmidev.h"
#include "ps_dev_if.h"

#include "hal_project.h"

/*====================================================
 * Build Configuration Print
 *====================================================*/

#define PRINT_ENABLE_STATUS(name, macro) \
    liot_trace("%-22s : %s", name, (macro) ? "Enable" : "Disable")

#define PRINT_STRING_STATUS(name, value) \
    liot_trace("%-22s : %s", name, value)


static const char* get_module_type(void)
{
#if MODULE_NT26KCNB00NNA
    return "NT26KCNB00NNA";
#elif MODULE_NT26KCNB00NNC
    return "NT26KCNB00NNC";
#elif MODULE_NT26K0B1
    return "NT26K0B1";
#elif MODULE_NT21KCNA
    return "NT21KCNA";
#else
    return "UNKNOWN";
#endif
}

static const char* get_product_type(void)
{
#if PRODUCT_SWITCH
    return "Switch Product";
#elif PRODUCT_MOTOR
    return "Motor Product";
#else
    return "UNKNOWN";
#endif
}

static const char* get_cloud_platform(void)
{
#if CMCC_ONENET
    return "CMCC OneNet";
#elif CT_AEP
    return "CT AEP";
#else
    return "UNKNOWN";
#endif
}

static const char* get_mem_location(void)
{
#if MEM_STATE_MCU_FLASH
    return "MCU Flash";
#elif MEM_STATE_CAT1_FLASH
    return "CAT1 Flash";
#else
    return "Not Enabled";
#endif
}

static const char* get_uart_protocol_format(void)
{
#if UART_PROTOCOL_FORMAT_HEX
    return "Hex";
#elif UART_PROTOCOL_FORMAT_ASCII
    return "ASCII";
#else
    return "None";
#endif
}


void app_print_build_config(void)
{
    liot_trace("====================================");
    liot_trace("        Build Configuration         ");
    liot_trace("====================================");

    liot_trace("Build Time                : %s %s", __DATE__, __TIME__);
    
    // 直接写逻辑，不要用宏封装打印
    liot_trace("Module Type               : %s", get_module_type());
    liot_trace("Product Type              : %s", get_product_type());
    liot_trace("Cloud Platform            : %s", get_cloud_platform());
    liot_trace("State Memory Location     : %s", get_mem_location());


    liot_trace("UART Communication        : %s", (UART_COMM_ENABLE) ? "Enable" : "Disable");
    liot_trace("UART Protocol Format      : %s", get_uart_protocol_format());


#if PRODUCT_SWITCH
    liot_trace("Switch IO Output          : %s", (IO_SWITCH_OUTPUT_ENABLE) ? "Enable" : "Disable");
    #if IO_SWITCH_OUTPUT_ENABLE
    liot_trace("Switch Mag Latch Pulse Out: %s", (MAG_LATCH_PULSE_OUTPUT_ENABLE) ? "Enable" : "Disable");
    liot_trace("Switch Four Channel Output: %s", (FOUR_CHANNEL_OUTPUT_ENABLE) ? "Enable" : "Disable");
    #endif
#elif PRODUCT_MOTOR
    liot_trace("Motor IO Output           : %s", (IO_MOTOR_OUTPUT_ENABLE) ? "Enable" : "Disable");
#endif

    liot_trace("Key Switch Input          : %s", (KEY_SWITCH_ENABLE) ? "Enable" : "Disable");
    #if KEY_SWITCH_ENABLE
    liot_trace("Key On/Off                : %s", (KEY_SWITCH_1_ON_OFF_ENABLE) ? "Enable" : "Disable");
    liot_trace("Key Four Channel          : %s", (KEY_SWITCH_2_3_4_ENABLE) ? "Enable" : "Disable");
    #endif

    liot_trace("Offline Auto Reset        : %s", (DEVICE_OFFLINE_RESET_ENABLE) ? "Enable" : "Disable");
    
    liot_trace("====================================");
}