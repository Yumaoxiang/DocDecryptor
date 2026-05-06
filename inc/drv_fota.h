#ifndef __DRV_FOTA_H__
#define __DRV_FOTA_H__


#define LIOT_FOTA_PACK_NAME_MAX_LEN_150     (128)      


typedef struct{
    uint16 url_len; 
    char url_address[LIOT_FOTA_PACK_NAME_MAX_LEN_150];
} cat1_fota_t;
extern cat1_fota_t cat1_fota;



void liot_fota_http_nvm_thread2();

#endif