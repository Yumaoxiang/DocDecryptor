/**
 * @File Name: liot_fota_http_demo.c
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_datacall.h"
#include "liot_dev.h"
#include "liot_fota.h"
#include "liot_fs_api.h"
#include "liot_http.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_power.h"
#include "liot_sockets.h"
#include "liot_type.h"

#include "hal_project.h"



cat1_fota_t cat1_fota;
char fota_url_address[LIOT_FOTA_PACK_NAME_MAX_LEN_150];

#define LIOT_FOTA_HTTP_LOG_LEVEL
#define LIOT_FOTA_HTTP_LOG                liot_trace
#define LIOT_FOTA_HTTP_LOG_PUSH           liot_trace
#define LIOT_FOTA_PACK_NAME_MAX_LEN       (64)
#define LIOT_FOTA_APPLICATION_DEMO_ENABLE (0)

#define LIOT_TRY_DOWN_TIMES             6
#define LIOT_WRITE_TO_FILESIZE          (1024 * 5)
#define LIOT_VERSION_MAX                256
#define LIOT_HTTP_HEAD_RANGE_LENGTH_MAX 50
#define LIOT_HTTP_DLOAD_URL             "https://xiot.oss.senthink.com/upload-http-ftp/17EDE081/V2.1.0_V2.2.0.par"
#define LIOT_FOTA_AP_FILE_NAME          "lierda_fota_package.bin"

static liot_task_t fota_http_task_nvm = NULL;
static liot_sem_t fota_http_semp_nvm  = NULL;
static liot_sem_t fota_http_semp_close = NULL;
typedef enum
{
    LIOT_FOTA_HTTP_DOWN_INIT,    //初始阶段
    LIOT_FOTA_HTTP_DOWN_DOWNING, //下载�?
    LIOT_FOTA_HTTP_DOWN_INTR,    //下载被中�?
    LIOT_FOTA_HTTP_DOWN_DOWNED,  //下载完成
    LIOT_FOTA_HTTP_DOWN_NOSPACE, //没有空间
} e_fota_down_stage_e;

typedef struct
{
    bool is_show;    //是否显示进度
    uint total_size; //文件总共大小
    uint dload_size; //已经下载大小
    uint file_size;  //上次升级中断保存的文件大�?
} fota_http_progress_s;

typedef struct
{
    liot_http_client_t http_cli; //和http交互创建的struct http_client_s类型的指�?
    liot_httpc_url_s http_url;
    bool b_is_http_range;                            //是否发送http get range 报文
    int profile_idx;                                 // cid
    uint8_t sim_id;                                  // simid
    char fota_packname[LIOT_FOTA_PACK_NAME_MAX_LEN]; //下载到本地的升级文件的位�?
    fota_http_progress_s http_progress;              // http进度
    e_fota_down_stage_e e_stage;                     // http下载固件包的阶段
    LFILE fd;                                        //写文件的文件描述�?
    int i_save_size; //在一次下载的过程中，分为多次写入，保存的是上次的写入大小，用于控制满多少字节写一�?
    uint last_precent;    //下载最后一次百分比
    bool b_is_have_space; //存储空间是否可用
    int chunk_encode;     // Transfer-Encoding:chunked 传输方式
} fota_http_client_s;


static void fota_http_close_fd(fota_http_client_s *fota_http_cli_p);

static int fota_dload_file_clran_nvm(fota_http_client_s *fota_http_cli_p){
    fota_http_cli_p->http_progress.file_size  = 0;
    fota_http_cli_p->http_progress.dload_size = 0;
    fota_http_cli_p->http_progress.total_size = 0;
    fota_http_cli_p->e_stage                  = LIOT_FOTA_HTTP_DOWN_DOWNING;
    fota_http_cli_p->i_save_size              = 0;
    LIOT_FOTA_HTTP_LOG("clran write file [%s] open fd %d", fota_http_cli_p->fota_packname, fota_http_cli_p->fd);
    return 0;
}

static void fota_http_info_cfg(fota_http_client_s *fota_http_cli_p){
    if (fota_http_cli_p == NULL)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_cli_p is null");
        return;
    }
    LIOT_FOTA_HTTP_LOG("init file stage:[%d]", fota_http_cli_p->e_stage);
    LIOT_FOTA_HTTP_LOG("init file download:[%d]", fota_http_cli_p->http_progress.dload_size);
    LIOT_FOTA_HTTP_LOG("init file file_size:[%d]", fota_http_cli_p->http_progress.file_size);
    LIOT_FOTA_HTTP_LOG("init file real file_size:[%ld]", liot_fota_nvm_free_size_get());
    LIOT_FOTA_HTTP_LOG("init file is_show:[%d]", fota_http_cli_p->http_progress.is_show);
    LIOT_FOTA_HTTP_LOG("init file last_percent:[%d]", fota_http_cli_p->last_precent);
    LIOT_FOTA_HTTP_LOG("init file space:[%d]", fota_http_cli_p->b_is_have_space);
}

/***********************************************************
 * funcname		:fota_http_event_cb
 * description	:
 *	http响应报文的回调函数，当http请求返回的时候调�?
 *	client		[in]	[http_client_t *] http句柄
 *	event		[in]    [http_event_id_e] http事件类型
 *	event_code  [in]    [http_error_code_e] http处理结果
 *	argv        [in]    [void *]		 liot_httpc_news传进来的参数
 *   infomation
 *
 ************************************************************/

static void fota_http_event_cb(liot_http_client_t *client, int event, int event_code, void *argv){
    if (argv == NULL)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_event_cb argv is null");
        return;
    }
    fota_http_client_s *fota_http_cli_p = (fota_http_client_s *)argv;
    LIOT_FOTA_HTTP_LOG("fota_http_event:%d,event_code:%d", event, event_code);

    switch (event)
    {
        case LIOT_HTTPC_SESSION_OPEN:
        {
            if (event_code != LIOT_HTTPC_SUCCESS)
            {
                LIOT_FOTA_HTTP_LOG("HTTP session create failed:%d!!!!!", event_code);
                //保存下载信息,如果不是无存储空间所致，设置为下载被中断状�?
                if (fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_NOSPACE ||
                    fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_DOWNED)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_INTR;
                }
                liot_rtos_semaphore_release(fota_http_semp_nvm);
            }
        }
        break;
        case LIOT_HTTPC_RESPONSE_STATUS:
        {
            if (event_code == LIOT_HTTPC_SUCCESS)
            {
                int resp_code      = 0;
                int content_length = 0;
                int chunk_encode   = 0;
                // int accept_ranges = 0;
                char *location = NULL;
                liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &resp_code);
                liot_httpc_getinfo(client, LIOT_HTTPC_CHUNK_ENCODE, &chunk_encode);
                LIOT_FOTA_HTTP_LOG("response code:%d chunk_encode %d", resp_code, chunk_encode);
                fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNING;
                if (resp_code == 200 || resp_code == 206)
                {
                    if (chunk_encode == 0)
                    {
                        // liot_httpc_getinfo(client, HTTP_INFO_ACCEPT_RANGES, &accept_ranges);
                        liot_httpc_getinfo(client, LIOT_HTTPC_CONTENT_LEN, &content_length);
                        // if(accept_ranges == 1 &&  fota_http_cli_p->b_is_http_range == TRUE)
                        if (fota_http_cli_p->b_is_http_range == TRUE)
                        {
                            fota_http_cli_p->http_progress.total_size += content_length;
                        }
                        else
                        {
                            if (fota_dload_file_clran_nvm(fota_http_cli_p) == 0)
                            {
                                fota_http_cli_p->http_progress.total_size = content_length;
                            }
                            else
                            {
                                fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                            }
                        }
                        LIOT_FOTA_HTTP_LOG("content_length:[%d] totalsize=[%d]",
                                           content_length,
                                           fota_http_cli_p->http_progress.total_size);
                    }
                    else if (1 == chunk_encode)
                    {
                        LIOT_FOTA_HTTP_LOG("http chunk encode!");
                        fota_http_cli_p->chunk_encode = 1;
                    }
                }
                else
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                }
                //返回�?416提示416 Requested Range Not Satisfiable
                if (resp_code == 416)
                {
                    //发送已经最大了
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                }
                if (resp_code >= 300 && resp_code < 400)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                    liot_httpc_getinfo(client, LIOT_HTTPC_LOCATION, &location);
                    LIOT_FOTA_HTTP_LOG("redirect location:%s", location);
                    free(location);
                }
            }
        }
        break;
        case LIOT_HTTPC_RESPONSE_TIMEOUT:
        case LIOT_HTTPC_RESPONSE_COMPLETE:
        {
            if (event_code == LIOT_HTTPC_SUCCESS)
            {
                //下载完成将配置文件设置为初始状�?,只允许此种情况才能恢复为初始状�?
                fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
                LIOT_FOTA_HTTP_LOG("===http transfer end!!!!");
            }
            else
            {
                //保存下载信息,如果不是无存储空间所致，设置为下载被中断状�?
                if (fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_NOSPACE &&
                    fota_http_cli_p->e_stage != LIOT_FOTA_HTTP_DOWN_DOWNED)
                {
                    fota_http_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_INTR;
                }
                LIOT_FOTA_HTTP_LOG("===http transfer occur exception!!!!!");
            }
            liot_rtos_semaphore_release(fota_http_semp_nvm);
        }
        break;
        case LIOT_HTTPC_SESSION_CLOSE:
        {
            liot_trace("http LIOT_HTTPC_SESSION_CLOSE success");
            liot_rtos_semaphore_release(fota_http_semp_close);
        }
    break;
    }
}
/***********************************************************
 * funcname		:fota_http_write_file
 * description	:
 *	将收到的数据写入文件�?
 ************************************************************/

static int fota_http_write_file(fota_http_client_s *fota_cli_p, char *data, int size, LFILE fd)
{
    int ret   = -1;
    uint temp = 0;
    // if (fota_cli_p->fd != fd)
    // {
    //     LIOT_FOTA_HTTP_LOG("file fd error");
    //     fota_http_close_fd(fota_cli_p);
    //     return 0;
    // }
    // ret = liot_fwrite(data, size, 1, fd);
    ret = liot_fota_nvm_write(fota_cli_p->http_progress.dload_size,(uint8_t *)data, size);
    LIOT_FOTA_HTTP_LOG("write size:[%d] dload_size=[%d]", size, fota_cli_p->http_progress.dload_size);
    if (ret >= 0)
    {
        fota_cli_p->http_progress.dload_size += (uint)size;
        // fota_cli_p->http_progress.file_size = liot_fsize(fd);
        if (fota_cli_p->http_progress.is_show == TRUE)
        {
            if (1 != fota_cli_p->chunk_encode)
            {
                //计算进度，如果开启进度显示，那么会计算本次进度和上次进度是否相同，进度不同才会展示进�?
                temp = 100UL * fota_cli_p->http_progress.dload_size / fota_cli_p->http_progress.total_size;
                if (fota_cli_p->last_precent != temp || temp == 100)
                {
                    fota_cli_p->last_precent = temp;
                    LIOT_FOTA_HTTP_LOG("dload progress:===[%u%%]===total size[%d] file_size[%d] dload size[%d]",
                                       temp,
                                       fota_cli_p->http_progress.total_size,
                                       liot_fsize(fd),
                                       fota_cli_p->http_progress.dload_size);
                }
            }
            else
            {
                LIOT_FOTA_HTTP_LOG("dload progress:=== file_size[%d] dload size[%d] ===",
                                   liot_fsize(fd),
                                   fota_cli_p->http_progress.dload_size);
            }
        }

        //保存文件，每一次满5k,保存一次写入的文件
        if ((fota_cli_p->i_save_size <= fota_cli_p->http_progress.dload_size) ||
            ((1 != fota_cli_p->chunk_encode) && (fota_cli_p->i_save_size >= fota_cli_p->http_progress.total_size)))
        {
            //刷新文件系统
            /*if(liot_fsync(fd) < LIOT_FS_OK)
            {
                LIOT_FOTA_HTTP_LOG("sync file failed");
                fota_http_close_fd(fota_cli_p);
                return 0;
            }*/

            //满LIOT_WRITE_TO_FILESIZE个字节保存一�?
            if ((1 != fota_cli_p->chunk_encode) && (fota_cli_p->i_save_size >= fota_cli_p->http_progress.total_size))
            {
                fota_cli_p->i_save_size = fota_cli_p->http_progress.total_size;
            }
            else
            {
                fota_cli_p->i_save_size = fota_cli_p->http_progress.dload_size + LIOT_WRITE_TO_FILESIZE;
            }
        }
        if ((1 != fota_cli_p->chunk_encode) &&
            (fota_cli_p->http_progress.dload_size >= fota_cli_p->http_progress.total_size))
        {
            fota_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
        }
        // if (ret != size)
        // {
        //     //关闭固件升级包的文件描述�?
        //     fota_http_close_fd(fota_cli_p);
        // }
    }
    else
    {
        LIOT_FOTA_HTTP_LOG("error: ret:%d", ret);
        //关闭固件升级包的文件描述�?
        fota_http_close_fd(fota_cli_p);
    }
    return ret;
}

/***********************************************************
 * funcname		:fota_http_write_response_data
 * description	:
 *	http响应报文，处理报文体的回调函数�?
 *	client		[in]  [int]   http请求的句�?
 *	argv		[in]  [void *] liot_httpc_setopt用HTTP_CLIENT_OPT_WRITE_DATA传递的参数
 *	data		[in ] [char *] http响应的报文体数据
 *	size		[in]  [int]    http响应报文体数据大�?
 *	end         [in]  [int]   1 表示当前为最后一包数�? 0 非最后一包数�?
 *   返回�?: 实际处理的数据长�?
 *
 ************************************************************/
static int fota_http_write_response_data(
    liot_http_client_t *client, void *argv, char *data, int size, unsigned char end)
{
    int ret            = -1;
    int write_size     = size;
    char *p_write_data = data;
    // int i_deal_size    = LIOT_WRITE_TO_FILESIZE;
    int file_free_size = 0;
    if (argv == NULL)
    {
        LIOT_FOTA_HTTP_LOG("fota_http_write_response_data argv is invalied NULL ");
        return -2;
    }
    fota_http_client_s *fota_cli_p = (fota_http_client_s *)argv;
    if ((fota_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_DOWNED) || (fota_cli_p->chunk_encode == 1 && end == 1))
    {
        fota_cli_p->e_stage = LIOT_FOTA_HTTP_DOWN_DOWNED;
        // 416提示资源已经超出长度，多余的314个字节的固件包数据不用写入文件�?
        LIOT_FOTA_HTTP_LOG("go on dload file finished", fota_cli_p->fota_packname);
        //关闭本地固件升级包文件描述符
        fota_http_close_fd(fota_cli_p);
        return 0;
    }

    //获取当前剩余空间，如果不够直接不写文�?
    file_free_size = liot_fota_nvm_free_size_get();
    if ((1 != fota_cli_p->chunk_encode &&
         file_free_size < (fota_cli_p->http_progress.total_size - fota_cli_p->http_progress.dload_size)) ||
        (1 == fota_cli_p->chunk_encode && file_free_size < size))
    {
        if (1 != fota_cli_p->chunk_encode)
        {
            LIOT_FOTA_HTTP_LOG("free_space[%d] total_size [%d] dload_size[%d]",
                               file_free_size,
                               fota_cli_p->http_progress.total_size,
                               fota_cli_p->http_progress.dload_size);
        }
        else
        {
            LIOT_FOTA_HTTP_LOG("free_space[%d] dload_size[%d]", file_free_size, fota_cli_p->http_progress.dload_size);
        }
        fota_cli_p->e_stage         = LIOT_FOTA_HTTP_DOWN_NOSPACE;
        fota_cli_p->b_is_have_space = FALSE;
        LIOT_FOTA_HTTP_LOG("file free_size not enough");
        fota_http_close_fd(fota_cli_p);
        return 0;
    }

    if (size <= 0)
    {
        LIOT_FOTA_HTTP_LOG("write 0 size to file [%s]", fota_cli_p->fota_packname);
        //关闭本地固件升级包文件描述符
        fota_http_close_fd(fota_cli_p);
        return -1;
    }
    //在这儿也可以添加一个缓存池，将数据先写入缓存池(需要判断数据量是否大于缓存池的情况)，当缓存池满了在写入文件�?
    //以防大量的小数据频繁写文件操�?
    //每次�?1k
    // do
    // {
    //     if (write_size < i_deal_size)
    //     {
    //         i_deal_size = write_size;
    //     }
        ret = fota_http_write_file(fota_cli_p, p_write_data, size, fota_cli_p->fd);
        if (ret < 0)
        {
            LIOT_FOTA_HTTP_LOG("write file error");
            return size - write_size;
        }
    //     write_size -= ret;
    //     p_write_data += ret;
    // } while (write_size > 0);
    return size;
}

static void fota_http_init_nvm(fota_http_client_s *fota_http_cli_p)
{
    // LFILE fd = -1;
    liot_rtos_semaphore_create(&fota_http_semp_nvm, 0);
    liot_rtos_semaphore_create(&fota_http_semp_close, 0);
    //获取上次未完成下载信�?
    memset(fota_http_cli_p, 0x00, sizeof(fota_http_client_s));

    fota_http_cli_p->http_cli    = 0; // http连接句柄
    fota_http_cli_p->profile_idx = 1; // cid
    fota_http_cli_p->sim_id      = 0; // simid
    fota_http_cli_p->e_stage     = LIOT_FOTA_HTTP_DOWN_INIT;
    fota_http_cli_p->i_save_size = 0;
    //存储空间开始默认为足够
    fota_http_cli_p->b_is_have_space       = TRUE;
    fota_http_cli_p->http_progress.is_show = FALSE; //设置展示进度�?
    fota_http_cli_p->last_precent          = 0;
    fota_http_cli_p->chunk_encode          = 0;

}

static void fota_http_release()
{
    liot_rtos_semaphore_release(fota_http_semp_nvm);
    liot_rtos_semaphore_delete(fota_http_semp_nvm);
    liot_rtos_semaphore_release(fota_http_semp_close);
    liot_rtos_semaphore_delete(fota_http_semp_close);
    //不设置成NULL 会导致restart
    fota_http_semp_nvm = NULL;
    fota_http_semp_close = NULL;
}
/***********************************************************
 * funcname		:fota_http_active
 * description	:
 *	注册网络，拨号，初始化网络环�?
 * return:
 *  LIOT_DATACALL_SUCCESS  -- sucess
 *  other 	-- failed
 ************************************************************/
static liot_datacall_errcode_e fota_http_active(uint8_t sim, int cid)
{
    liot_datacall_errcode_e ret        = LIOT_DATACALL_SUCCESS;
    liot_nw_reg_status_info_s reg_info = {0};
    liot_nw_get_reg_status(sim, &reg_info);
    while (LIOT_NW_REG_STATE_HOME_NETWORK != reg_info.data_reg.state)
    {
        LIOT_FOTA_HTTP_LOG("wait for network reg:%d", reg_info.data_reg.state);
        liot_rtos_task_sleep_ms(500);
        liot_nw_get_reg_status(sim, &reg_info);
    }
    liot_trace("===start data call====");
    ret = liot_start_data_call(sim, cid, LIOT_DATA_TYPE_IP, "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("===data call result:%d", ret);

    liot_rtos_task_sleep_s(10);
    return LIOT_DATACALL_SUCCESS;
}

/********************************************************************
 * funcname		:fota_http_get_fd
 * description	:
 *	打开升级包文件描述符，用于写入http下载升级包数据，
 *	如果读取的上次保留的下载信息配置文件中的中断状态是初始状态、下�?
 *	已完成状态、或者是由于存储空间不够导致的下载失败状态，则删除上级包
 *	重新下载重新覆盖写本地上次下载的升级包文件，其他的中断情况则启用�?
 *	加的方式重新请求升级包，追加升级包文件�?
 *  fota_http_cli_p [in]   [fota_http_client_s *] fota http客户端结构体
 * return:
 *  0  -- sucess
 *  other 	-- failed
 **********************************************************************/
static int fota_http_cli_init(fota_http_client_s *fota_http_cli_p)
{
    if (fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_INIT ||
        fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_DOWNED ||
        fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_NOSPACE)
    {
        //已经完成、初始化、或者是没有空间导致的下载中断以覆盖写来打开文件
        fota_http_cli_p->http_progress.file_size  = 0;
        fota_http_cli_p->http_progress.dload_size = 0;
        fota_http_cli_p->http_progress.total_size = 0;
        fota_http_cli_p->e_stage                  = LIOT_FOTA_HTTP_DOWN_INIT;
        fota_http_cli_p->i_save_size              = 0;
        LIOT_FOTA_HTTP_LOG("over write file");
    }
    else
    {
        //其他的情况是追加方式打开文件
        LIOT_FOTA_HTTP_LOG("add write file");
    }
    return 0;
}
/********************************************************************
 * funcname		:fota_http_close_fd
 * description	:
 *	关闭下载升级包的文件描述�?
 *  fota_http_cli_p [in]   [fota_http_client_s *] fota http客户端结构体
 * return:
 *  0  -- sucess
 *  other 	-- failed
 **********************************************************************/

static void fota_http_close_fd(fota_http_client_s *fota_http_cli_p)
{
    // if (fota_http_cli_p->fd > 0)
    // {
    //     liot_fclose(fota_http_cli_p->fd);
    //     fota_http_cli_p->fd = -1;
    // }
}

static void liot_http_url_free(liot_httpc_url_s *url)
{
    if(url->host)
    {
        liot_rtos_free(url->host);
    }

    if(url->uri)
    {
        liot_rtos_free(url->uri);
    }

}
extern void liot_http_info_log(liot_http_client_t *client);
/***********************************************************
 * funcname		:fota_http_active
 * description	:
 *	初始化http请求的网络环境，组建http请求报文，发起http请求
 * return:
 *  0  -- sucess
 *  other 	-- failed
 ************************************************************/
static int fota_http_evn_request(fota_http_client_s *fota_http_cli_p)
{
    char dload_range[LIOT_HTTP_HEAD_RANGE_LENGTH_MAX] = {0};
    liot_httpc_method_e e_http_method;
    int cid = fota_http_cli_p->profile_idx;

    liot_httpc_url_s local_http_url;
    //发送http请求前创建存储升级包文件的文件描述符，别忘关�?
    if (fota_http_cli_init(fota_http_cli_p) < 0)
    {
        LIOT_FOTA_HTTP_LOG("range_request http data done ,file_size[%d]", fota_http_cli_p->http_progress.file_size);
        return -1;
    }
    memset(&local_http_url, 0x00, sizeof(local_http_url));
    // 地址解析成功后，该接口内部会给liot_httpc_url_s 中的uri与host申请内存，如果单独调用，则
    // 需要在申请完主动释放一次。如果配合liot_httpc_setopt接口进行url设置，则无法释放，内部会进行内存释放。
    if (TRUE != liot_httpc_url_parse(fota_url_address, &(local_http_url)))
    {
        LIOT_FOTA_HTTP_LOG("fota-url-parse fail!!!");
        fota_http_close_fd(fota_http_cli_p);
        return -1;
    }

    //创建http请求句柄
    if (liot_httpc_new(&(fota_http_cli_p->http_cli), fota_http_event_cb, fota_http_cli_p) != LIOT_HTTPC_SUCCESS)
    {
        LIOT_FOTA_HTTP_LOG("http create failed");
        liot_httpc_release(&(fota_http_cli_p->http_cli));
        liot_http_url_free(&(local_http_url));
        fota_http_close_fd(fota_http_cli_p);
        return -2;
    }
    //设置http请求方式为HTTP_METHOD_GET
    e_http_method = LIOT_HTTPC_METHOD_GET;
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_METHOD, e_http_method);
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_PDPCID, cid);

    //文件大小不为0，fota_http_get_fd已经限制了只有在下载被中断的情况下发�?
    if (fota_http_cli_p->b_is_http_range == TRUE)
    {
        LIOT_FOTA_HTTP_LOG("dload_range");
        //设置使用断电续传功能，使用上次下载未完成的最后下载信�?
        fota_http_cli_p->http_progress.dload_size = fota_http_cli_p->http_progress.file_size;
        sprintf(dload_range, "Range: bytes=%d-", fota_http_cli_p->http_progress.file_size);
        liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, dload_range);
        LIOT_FOTA_HTTP_LOG("Get http %s", dload_range);
    }
    else
    {
        //不设置范围下载字�?
    }
    //设置url下载地址
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_URL, &(local_http_url));
    liot_http_url_free(&(local_http_url));
    //设置sim_id
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_SIM_ID, fota_http_cli_p->sim_id);
    //设置cid
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_PDPCID, fota_http_cli_p->profile_idx);
    //接收报体中的文件内容
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, fota_http_write_response_data);
    //设置fota_http_write_response_data 第二参数为fota_http_cli
    liot_httpc_setopt(&(fota_http_cli_p->http_cli), LIOT_HTTP_CLIENT_OPT_WRITE_DATA, fota_http_cli_p);
    //发送http请求
    if (liot_httpc_perform(&fota_http_cli_p->http_cli) == LIOT_HTTPC_SUCCESS)
    {
        //阻塞等待信号�?
        if (liot_rtos_semaphore_wait(fota_http_semp_nvm, LIOT_WAIT_FOREVER) != LIOT_OSI_SUCCESS)
        {
            //获取信号量失�?
            liot_httpc_stop(&(fota_http_cli_p->http_cli));
            liot_rtos_semaphore_wait(fota_http_semp_close, LIOT_WAIT_FOREVER);
            liot_httpc_release(&(fota_http_cli_p->http_cli));
            fota_http_close_fd(fota_http_cli_p);
            LIOT_FOTA_HTTP_LOG("liot_rtos_semaphore_wait != LIOT_OSI_SUCCESS fail");
            return -1;
        }
        LIOT_FOTA_HTTP_LOG("fota http dload size %d=====End,\n", fota_http_cli_p->http_progress.dload_size);
        liot_httpc_stop(&(fota_http_cli_p->http_cli));
        // 在发送stop之后，接口内部属于异步操作，等到完全关闭socket之后才可以进行资源释放，否则会导致内存泄漏。
        liot_rtos_semaphore_wait(fota_http_semp_close, LIOT_WAIT_FOREVER);
        liot_httpc_release(&(fota_http_cli_p->http_cli));
        LIOT_FOTA_HTTP_LOG("http release sucess!!!");
        fota_http_close_fd(fota_http_cli_p);
        return 0;
    }
    liot_httpc_release(&(fota_http_cli_p->http_cli));
    fota_http_close_fd(fota_http_cli_p);
    LIOT_FOTA_HTTP_LOG("fota_http_evn_request end fail!!!");
    return -3;
}

/***********************************************************
 * funcname		:fota_http_download_pacfile
 * description	:
 *	下载升级包主体函�?
 * return:
 *  0 		-- sucess  下载成功并且校验成功
 *
 *  other 	-- failed
 ************************************************************/
static int fota_http_download_pacfile(fota_http_client_s *fota_http_cli_p)
{
    fota_http_info_cfg(fota_http_cli_p);
    //初始化http网络环境，组http请求报文,发送http请求，阻塞到下载完成或异�?
    if (fota_http_evn_request(fota_http_cli_p) != 0)
    {
        int file_size = liot_fota_nvm_free_size_get();
        LIOT_FOTA_HTTP_LOG("liot_fota_nvm_free_size_get size[%d]", file_size);
        return -1;
    }
    fota_http_info_cfg(fota_http_cli_p);
    //校验下载完成文件是否有效
    if (fota_http_cli_p->e_stage == LIOT_FOTA_HTTP_DOWN_DOWNED)
    {
        int ret = liot_fota_nvm_image_verify();
        if (ret != LIOT_FOTA_UPGRADE_SUCCESS)
        {
            //下载完成校验不成功删除文�?
            liot_fota_clear(NULL,TRUE);
            LIOT_FOTA_HTTP_LOG("[%s]package is invalid", fota_http_cli_p->fota_packname);
            return -3;
        }
        else
        {
            //校验成功
            LIOT_FOTA_HTTP_LOG("download is sucess ,system will reset power!");
            liot_rtos_task_sleep_s(5);
            liot_fota_power_reset(LIOT_RESET_NORMAL);
        }
    }
    return 0;
}

static liot_fota_result_e fota_http_result_process_nvm(void)
{
    liot_fota_result_e p_fota_result = 0;

    //获取升级结果
    if (liot_fota_get_result(&p_fota_result) != LIOT_FOTA_UPGRADE_SUCCESS)
    {
        LIOT_FOTA_HTTP_LOG("liot_fota_get_result failed ");
        return LIOT_FOTA_STATUS_INVALID;
    }

    if (p_fota_result == LIOT_FOTA_FINISHED)
    {
        LIOT_FOTA_HTTP_LOG("update finished");
        // liot_fota_file_reset(TRUE);
        return LIOT_FOTA_FINISHED;
    }
    else if (p_fota_result == LIOT_FOTA_UPGRADE_READY)
    {
        LIOT_FOTA_HTTP_LOG("fota ready bigen power reset ");
        liot_rtos_task_sleep_s(5);
        liot_power_reset(LIOT_RESET_NORMAL);
    }
    else if (p_fota_result == LIOT_FOTA_NOT_EXIST)
    {
        LIOT_FOTA_HTTP_LOG("fota file not exist");
        // liot_fota_file_reset(TRUE);
        return LIOT_FOTA_NOT_EXIST;
    }
    LIOT_FOTA_HTTP_LOG("fota  result status invalid");
    return LIOT_FOTA_STATUS_INVALID;
}


void liot_fota_http_nvm_thread2(){
    // 要赋值的字符串
    // const char *url = "https://xiot.oss.senthink.com/upload-http-ftp/17EDE081/V2.1_V2.2.par";
    uint16_t url_length = strlen(cat1_fota.url_address);                      // 计算字符串的长度，并确保不超过目标数组的大小
    LIOT_FOTA_HTTP_LOG("url_length: %d", url_length);       // 打印结果
    if (url_length >= LIOT_FOTA_PACK_NAME_MAX_LEN_150) {
        url_length = LIOT_FOTA_PACK_NAME_MAX_LEN_150 - 1; // 留出空间给'\0'
    }
    LIOT_FOTA_HTTP_LOG("url_length: %d", url_length);// 打印结果

    memset(fota_url_address, 0, sizeof(fota_url_address));        // 将数组初始化为全0
    strcpy(fota_url_address, cat1_fota.url_address);              // 使用 strcpy以'\0'结束复制
    LIOT_FOTA_HTTP_LOG("FOTA URL Address: %s", fota_url_address);// 打印结果

    LIOT_FOTA_HTTP_LOG("init file real file_size:[%ld]", liot_fota_nvm_free_size_get());
    fota_http_client_s fota_http_cli;
    uint8 ui_down_times                = LIOT_TRY_DOWN_TIMES;
    char version_buf[LIOT_VERSION_MAX] = {0};
    //获取升级结果
    if (fota_http_result_process_nvm() == LIOT_FOTA_FINISHED)
    {
        LIOT_FOTA_HTTP_LOG("Fota file and flag clear!!!");
    }

    INT32 file_free_size = liot_fota_nvm_free_size_get();
    LIOT_FOTA_HTTP_LOG("fs free size get:0x%x", file_free_size);
    liot_rtos_task_sleep_s(2);
    liot_dev_get_firmware_version(version_buf, sizeof(version_buf));
    LIOT_FOTA_HTTP_LOG("current version:  %s", version_buf);
    //下载前初始化
    fota_http_init_nvm(&fota_http_cli);
     //注网拨号
    if (LIOT_DATACALL_SUCCESS != fota_http_active(fota_http_cli.sim_id, fota_http_cli.profile_idx))
    {
        LIOT_FOTA_HTTP_LOG("http net is failed ");
        goto exit;
    }
    //尝试下载最多十�?
    while (ui_down_times--)
    {
        LIOT_FOTA_HTTP_LOG("start [%d] times download fota packge", LIOT_TRY_DOWN_TIMES - ui_down_times);
        if(liot_fota_nvm_init() !=  LIOT_FOTA_UPGRADE_SUCCESS)
        {
            LIOT_FOTA_HTTP_LOG("liot_fota_nvm_init failed");
            break;
        }
        if (fota_http_download_pacfile(&fota_http_cli) == 0)
        {
            //下载完成
            break;
        }

        if (fota_http_cli.b_is_have_space != TRUE)
        {
            //空间不够，删除文�?
            liot_fota_clear(NULL,TRUE);
            LIOT_FOTA_HTTP_LOG("have no space");
            //break;
        }
        //下载失败等待10s重新开始下�?

        liot_rtos_task_sleep_ms(2000);
    }
    //下载失败等待10s重新开始下�?
exit:
    fota_http_release();
    // liot_rtos_task_sleep_s(500);
    LIOT_FOTA_HTTP_LOG("exit liot_http_fota_demo,%d", fota_http_task_nvm);
    liot_rtos_task_delete(fota_http_task_nvm);
}
