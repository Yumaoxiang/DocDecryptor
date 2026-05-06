#ifndef __HAL_FLASH_H__
#define __HAL_FLASH_H__


//分区头文件定义在 mem_map_716s.h中
#define FLASH_USER_REGION_START   (0x1e5000)
#define FLASH_USER_REGION_END     (0x1e6000)
#define FLASH_USER_REGION_SIZE    (FLASH_USER_REGION_END-FLASH_USER_REGION_START)
#define FLASH_READ_WRITE_LEN_32 32

extern char flash_write_data[FLASH_READ_WRITE_LEN_32];  // 待写入的数据
extern char flash_read_data[FLASH_READ_WRITE_LEN_32];   // 读取的数组



typedef enum {                        
    enSTATUS_PWR_OUT_H='H',       
    enSTATUS_PWR_OUT_L='L',			   
    enSTATUS_PWR_OUT_HOLD='P',
} e_STATUS_PWR_OUT;

// "off0000_reboot:0_init:1_sRst:0"
typedef struct flash_info {
    char mode[3+1];             // o_n / off / pre
    char state[4+1];            // 0000 / 0001  
    char reboot[8+1];           // 重启次数记录
    char init[6+1];             // 是否写入过标志位，如果没有写入过，上电立即写入1次
    char softreset[6+1];        // 软件复位标志位，liot_get_powerup_reason(&pwr_reason)无法区分，只能手动写入
} flash_poweron_t;
extern flash_poweron_t flash_poweron;


void FlashRead();
void FlashRead_PowerOn();
void FlashRead_PowerOn_extract(char *str);
uint8 FlashRead_PowerOn_Init();
void FlashRead_PowerOn_IOSet();

uint8 FlashWrite_PowerOnStatus(e_STATUS_PWR_OUT status);
unsigned char FlashRead_CheckModeSuc(e_STATUS_PWR_OUT status);
uint8 FlashWrite_IOSwitch();
void FlashWriteData_UpdateIOStatus();
uint8 FlashWrite_softwareReset();
uint8 FlashWrite_reboot(uint8 flag);
uint8 FlashWrite_And_Check(uint8 times);

#endif
