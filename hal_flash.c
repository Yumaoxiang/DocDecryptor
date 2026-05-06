#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_flash.h"
#include "liot_os.h"
#include "liot_type.h"
#include "mem_map.h"

#include "hal_project.h"



char flash_write_data[FLASH_READ_WRITE_LEN_32] = "off0000_reboot:0_init:1_sRst:0";  // 待写入的数据
char flash_read_data[FLASH_READ_WRITE_LEN_32] = {0};                                // 读取的数组
flash_poweron_t flash_poweron = {0};


/**********************************************************************
 * 名    称：FlashRead()
 * 功    能：封装，便于其他.c文件调用
 * 说    明：
 *********************************************************************/
void FlashRead(){
    liot_flash_read((uint8_t *)(&flash_read_data[0]), FLASH_USER_REGION_START, FLASH_READ_WRITE_LEN_32);
}

/**********************************************************************
 * 名    称：FlashRead_PowerOn()
 * 功    能：Flash上电读取对应状态，确认IO口状态
 * 说    明：
 * 修    改：24-12-29 修改Flash读写数据内容 Flash次数修改为重启次数，同时预留其他数据位置 "off0000_reboot:0_write:00000"; 
 *********************************************************************/
void FlashRead_PowerOn(){
    FlashRead();
    liot_trace("FlashRead_PowerOn(): %s", flash_read_data);
    FlashRead_PowerOn_extract(flash_read_data);
    FlashRead_PowerOn_Init();
    FlashRead_PowerOn_IOSet();
}

/**********************************************************************
 * 名    称：FlashRead_PowerOn_extract()
 * 功    能：解析读取到的Flash内容
 * 入口参数：str Flash读取到带解析的字符串
 * 说    明：解析后的内容存放在flash_poweron结构体中，Flash记录格式如下
 * 修    改：25-01-05 根据新的字符串重新调整位置："off0000_reboot:0_init:1_sRst:0"
 *********************************************************************/
void FlashRead_PowerOn_extract(char *str){
    memset(&flash_poweron, 0, sizeof(flash_poweron));   // 获取之前，先清0
    
    memcpy(flash_poweron.mode, str, 3);                 // strncpy(flash_poweron.mode, &str[0], 3); 等效于strncpy(sub, str, 3)
    flash_poweron.mode[3] = '\0';                       // Add null terminator to make it a valid C string
    
    memcpy(flash_poweron.state, str + 3, 4);            // strncpy(flash_poweron.state, &str[3], 4); 等效于strncpy(sub, str+3, 4)
    flash_poweron.state[4] = '\0';                      // Add null terminator to make it a valid C string

    memcpy(flash_poweron.reboot, str + 8, 8);                  
    flash_poweron.reboot[8] = '\0';                              

    memcpy(flash_poweron.init, str + 17, 6);                
    flash_poweron.init[6] = '\0';                            

    memcpy(flash_poweron.softreset, str + 24, 6);          
    flash_poweron.softreset[6] = '\0';                           

     // 格式化输出日志
    liot_trace("flash_poweron.mode is: %s, flash_poweron.state is: %s, flash_poweron.reboot is: %s, flash_poweron.init is: %s, flash_poweron.softreset is: %s",
               flash_poweron.mode, flash_poweron.state, flash_poweron.reboot, flash_poweron.init, flash_poweron.softreset);
}

/**********************************************************************
 * 名    称：FlashRead_PowerOn_Init()
 * 功    能：如果默认是刚烧录的固件，更新内容
 * 入口参数：str Flash读取到带解析的字符串
 * 出口参数：1 读取与写入一致     0 读取与写入不一致
 * 说    明："off0000_reboot:0_init:1_sRst:0"
 *          因为SDK无法检测是否为软件复位，在复位前写入Flash，然后上电后读取，这样就需要每次判断是否清0，在这里执行
 * 修    改：25-01-05 优化代码结构，将各个功能模块拆分
 *********************************************************************/
uint8 FlashRead_PowerOn_Init(){
    // 如果已经记录过Flash
    if(strcmp(flash_poweron.init, "init:1") != 0){              // 如果没有写入过Flash
        liot_trace("FlashRead_PowerOn_Init() is defaut value...");
        FlashRead_PowerOn_extract(flash_write_data);            // 重新提取默认值，保证后面其他位置可以直接判断
        return FlashWrite_And_Check(3);
    }
    liot_trace("FlashRead_PowerOn_Init() alreay write before!");

    if(strcmp(flash_poweron.softreset, "sRst:1") == 0){         // 如果写入过softReset标志位1
        liot_trace("FlashRead_PowerOn_Init() pre is reset, clear Flag to '0'");
        for(uint8 i=0; i<FLASH_READ_WRITE_LEN_32; i++){
            flash_write_data[i] = flash_read_data[i];
        }
        liot_trace("set to 'sRst:0' and 'cat1.pwrReason=SFT' ");
        flash_write_data[29] = '0';                             // sRst:0"
        strcpy(cat1.pwrReason, "SFT");                          // 同时修改上电信息，保证平台可收到信息
        return FlashWrite_And_Check(3);
    }
    liot_trace("FlashRead_PowerOn_Init() not software reset, return.");

    return 1;
}

/**********************************************************************
 * 名    称：FlashRead_PowerOn_IOSet()
 * 功    能：Flash上电设置IO输出状态
 * 说    明：
 * 修    改：
 *********************************************************************/
void FlashRead_PowerOn_IOSet(){
#if MEM_STATE_CAT1_FLASH
#if PRODUCT_SWITCH
    liot_trace("FlashRead_PowerOn_IOSet() for PRODUCT_SWITCH MEM_STATE_CAT1_FLASH");
    if(strcmp(flash_poweron.mode, "off") == 0){
        for (int i = 0; i < 4; i++) {
            g_io.out[i].pre_status = enIO_OUT_OFF;          // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
            g_io.out[i].now_status = enIO_OUT_OFF;          // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
        }
        update_io_state_from_g_io();                        // 根据Flash状态设置IO口状态
        liot_trace("PowerOn status is OFF, set all IO to OFF");
    }
    else if(strcmp(flash_poweron.mode, "o_n") == 0){
        for (int i = 0; i < 4; i++) {
            g_io.out[i].pre_status = enIO_OUT_ON;           // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
            g_io.out[i].now_status = enIO_OUT_ON;           // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
        }
        update_io_state_from_g_io();                        // 根据Flash状态设置IO口状态
        liot_trace("PowerOn status is ON, set all IO to ON");
    }
    else if (strcmp(flash_poweron.mode, "pre") == 0) {          // 状态保持需要根据之前的状态来设置
        for (int i = 0; i < 4; i++) {
            if (flash_poweron.state[i] == '0') {
                g_io.out[i].pre_status = enIO_OUT_OFF;
                g_io.out[i].now_status = enIO_OUT_OFF;
            } else if (flash_poweron.state[i] == '1') {
                g_io.out[i].pre_status = enIO_OUT_ON;
                g_io.out[i].now_status = enIO_OUT_ON;
            } else {
                liot_trace("Unknown state in Flash for IO%d, default to OFF", 4 - i);
                g_io.out[i].pre_status = enIO_OUT_OFF;
                g_io.out[i].now_status = enIO_OUT_OFF;
            }
        }
        update_io_state_from_g_io();                        // 根据Flash状态设置IO口状态
        liot_trace("PowerOn status is HOLD_PREVIOUS, set IO according to Flash record");
    }
    else{
        liot_trace("Unknown PowerOn status in Flash, set all IO to OFF");
        for (int i = 0; i < 4; i++) {
            g_io.out[i].pre_status = enIO_OUT_OFF;          // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
            g_io.out[i].now_status = enIO_OUT_OFF;          // GPIO上电后默认关闭，读取Flash后的状态在该初始化顺序之后
        }
        update_io_state_from_g_io();                        // 根据Flash状态设置IO口状态
    }

#endif    
#endif
}


/*******************************************************************************
 * 名    称：FlashWrite_PowerOnStatus
 * 功    能: 平台下发上电状态设置命令，保存到Flash
 * 入口参数：e_STATUS_PWR_OUT H/L/P    
 * 出口参数：1 写入成功   0 写入失败
 * 修    改：24-12-27 修改默认字符串"o_n000065535"-->"off0000_reboot:0_init:1_sRst:0"
 *******************************************************************************/
uint8 FlashWrite_PowerOnStatus(e_STATUS_PWR_OUT status){
    // Step1: 读取原本服务器存储的数据
    FlashRead();
    liot_trace("FlashWrite_PowerOnStatus(): %s", flash_read_data);

    // Step2: 设置"上电状态"
    switch(status){
    case enSTATUS_PWR_OUT_H:
        flash_write_data[0] = 'o';
        flash_write_data[1] = '_';
        flash_write_data[2] = 'n';
        break;

    case enSTATUS_PWR_OUT_L:
        flash_write_data[0] = 'o';
        flash_write_data[1] = 'f';
        flash_write_data[2] = 'f';
        break;

    case enSTATUS_PWR_OUT_HOLD:
        flash_write_data[0] = 'p';
        flash_write_data[1] = 'r';
        flash_write_data[2] = 'e';
        FlashWriteData_UpdateIOStatus();
        break;

    default:                                        // 保持Flash原有数据
        liot_trace("Unknown server cmd set power state.");
        return 0;                                   // 不是对应的命令，返回写入失败标志
    }

    //Step3：其他数据保持不变      // ：确认是否写入的位置正确liot_trace("%c", );
    for(uint8 i=7; i<FLASH_READ_WRITE_LEN_32; i++){
        flash_write_data[i] = flash_read_data[i];
    }

    return FlashWrite_And_Check(3);
}

/*******************************************************************************
 * 名  称：FlashWrite_IOSwitch
 * 功  能: IO变化的时候写Flash
 * 入口参数：staPowerOn: 上电状态    staIO：开/关
 * 说  明: 只有状态保持才需要写Flash，其他状态(上电开、关)单独设置即可
 *         每次服务器下发命令，必须更新sysPara.Flash_data_Read[0](读取,或者换一个变量)
 * 修   改：25-01-02 修改写入数据格式，匹配字符串
 *******************************************************************************/
uint8 FlashWrite_IOSwitch(){
    if(!(strcmp(flash_poweron.mode, "pre") == 0)){                          // 如果不是"状态保持"
        liot_trace("not HOLD_PREVIOUS, no write to Flash"); 
        return 1;
    }

    // Step1: 读取原本服务器存储的数据
    FlashRead();
    liot_trace("FlashWrite_IOSwitch(): %s", flash_read_data);

    // Step2: 更新IO口
    flash_write_data[0] = 'p';
    flash_write_data[1] = 'r';
    flash_write_data[2] = 'e';
    FlashWriteData_UpdateIOStatus();

    //Step3：其他数据保持不变      
    for(uint8 i=7; i<FLASH_READ_WRITE_LEN_32; i++){
        flash_write_data[i] = flash_read_data[i];
    }
    
    //Step4：写入数据并读取校验
    return FlashWrite_And_Check(3);
}

/*******************************************************************************
 * 名  称：FlashWrite_softwareReset
 * 功  能: 准备软件复位，写入Flash，便于复位后进行辨别
 * 入口参数：
 * 说  明: 
 * 修   改：
 *******************************************************************************/
uint8 FlashWrite_softwareReset(){
    // Step1: 读取原本服务器存储的数据
    FlashRead();
    liot_trace("FlashWrite_softwareReset() read flash: %s", flash_read_data);

    // Step2: 保持原始数据，同时更新标志位
    for(uint8 i=0; i<FLASH_READ_WRITE_LEN_32; i++){
        flash_write_data[i] = flash_read_data[i];
    }
    flash_write_data[29] = '1';                             // "sRst:1"
    // Step3：写入数据并读取校验
    return FlashWrite_And_Check(3);
}

/**********************************************************************
 * 名    称：FlashWrite_rebootclear()
 * 功    能：清除reboot写入信息
 * 入口参数：flag  0 清除    1 准备复位
 * 出口参数：1 读取与写入一致     0 读取与写入不一致
 * 说    明："off0000_reboot:0_init:1_sRst:0"
 *          只允许在AEP初次驻网成功后调用！！！
 * 修    改：
 *********************************************************************/
uint8 FlashWrite_reboot(uint8 flag){
    // 统一读取flash并处理数据
    FlashRead();
    liot_trace("FlashWrite_softwareReset() read flash: %s", flash_read_data);

    // Step1: 复制数据并更新标志位
    memcpy(flash_write_data, flash_read_data, FLASH_READ_WRITE_LEN_32);

    if (flag) {                             // 执行复位操作
        flash_write_data[29] = '1';         // "sRst:1"
        switch (flash_write_data[15]) {     // 更新标志位，避免多个if语句
            case '0': flash_write_data[15] = '1'; break;
            case '1': flash_write_data[15] = '2'; break;
            case '2': flash_write_data[15] = '3'; break;
            default: break;                 // 如果是其他值，保持不变
        }

        return FlashWrite_And_Check(3);     // Step2：写入数据并读取校验
    }

    else {                                  // 执行清除操作
        if (strcmp(flash_poweron.reboot, "reboot:0") == 0) { // 如果已经是reboot:0，则无需清除
            liot_trace("'reboot:0', return...");
            return 1;
        }
        liot_trace("FlashWrite_softwareReset() is not 'reboot:0', clear!!!");
        flash_write_data[15] = '0';         // "reboot:0"

        return FlashWrite_And_Check(3);     // Step2：写入数据并读取校验
    }

    return 1;                               // 默认返回值，理论上不会执行到这里
}

/*******************************************************************************
 * 名  称：FlashWriteData_UpdateIOStatus
 * 功  能: 更新4个IO口的状态
 * 说  明: 
 *******************************************************************************/
void FlashWriteData_UpdateIOStatus(){
    // flash_write_data[6] = (sysPara.out1.now_status == enIO_OUT_ON) ? '1' : '0';

    #if defined(MUTI_OUT4)
    flash_write_data[5] = (sysPara.out2.now_status == enIO_OUT_ON) ? '1' : '0';
    flash_write_data[4] = (sysPara.out3.now_status == enIO_OUT_ON) ? '1' : '0';
    flash_write_data[3] = (sysPara.out4.now_status == enIO_OUT_ON) ? '1' : '0';
    #endif

    liot_trace("FlashWriteData_UpdateIOStatus '%s'", flash_write_data); 
}

/**********************************************************************
 * 名    称：FlashWrite_Check
 * 功    能: 检查写入的数据是否正确
 * 入口参数：times 最多写入的次数
 * 返回值：1 成功    0 失败
 *********************************************************************/
uint8 FlashWrite_And_Check(uint8 times) {
    flash_write_data[FLASH_READ_WRITE_LEN_32 -1] = '\0';                                
    
    for (uint8 i = 1; i <= times; i++) {
        liot_flash_erase(FLASH_USER_REGION_START, FLASH_USER_REGION_SIZE);
        liot_rtos_task_sleep_ms(20);
        liot_flash_write((uint8_t *)flash_write_data, FLASH_USER_REGION_START, FLASH_READ_WRITE_LEN_32);

        liot_flash_read((uint8_t *)flash_read_data, FLASH_USER_REGION_START, FLASH_READ_WRITE_LEN_32);
        liot_trace("FlashWrite_And_Check() write %d times, write( %s ), read( %s )", i, flash_write_data, flash_read_data);

        if (memcmp(flash_write_data, flash_read_data, FLASH_READ_WRITE_LEN_32 - 2) == 0) {
            liot_trace("***** read = write, success *****"); 
            return 1;
        }
    }
    liot_trace("!!!!! read != write, success !!!!!"); 

    return 0;
}

/**********************************************************************
 * 名    称：FlashRead_Check()
 * 功    能：Flash写入后，加入读功能，检查写入信息是否正确
 * 入口参数：H L P
 * 返回值：  1 成功   0 失败
 * 说    明：-------------------------
 *           模式 | IO开关 | Flash次数
 *          -----|--------|----------
 *           o_n |        |
 *          -----|  0000  |
 *           off |        |  xxxxx
 *          -----|  0001  |
 *           pre |        |
 *          --------------------------
 *********************************************************************/
unsigned char FlashRead_CheckModeSuc(e_STATUS_PWR_OUT status){
    const char *expected_mode = NULL;       // 定义状态对应的模式字符串
    char read_buff[FLASH_READ_WRITE_LEN_32] = {0};
    
    liot_flash_read((uint8_t *)(&read_buff[0]), FLASH_USER_REGION_START, FLASH_READ_WRITE_LEN_32);
    liot_trace("FlashRead_Check is: %s", read_buff);
    FlashRead_PowerOn_extract(read_buff);
    
    switch (status) {
        case enSTATUS_PWR_OUT_H:
            expected_mode = "o_n";
            break;
        case enSTATUS_PWR_OUT_L:
            expected_mode = "off";
            break;
        case enSTATUS_PWR_OUT_HOLD:
            expected_mode = "pre";
            break;
        default:
            liot_trace("write poweron status default fail.");
            return 0;
    }
    
    if (strcmp(flash_poweron.mode, expected_mode) == 0) { // 检查模式是否匹配
        liot_trace("write poweron status '%s' check success.", expected_mode);
        return 1;
    } else {
        liot_trace("write poweron status '%s' check fail.", expected_mode);
        return 0;
    }
}


