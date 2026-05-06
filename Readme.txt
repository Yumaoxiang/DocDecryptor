
版本记录：
   V0.1 初始版本                                                           24-06-09 
   V0.2 封装发送函数，失败时做异常处理                                       24-06-13
        定于函数aep_mqtt_reg()中不在做异常处理，而是交给上层
   V0.3 更新上电状态命令为Open/Clos/Hold                                    24-06-16
        心跳包增加timerInfo信息
   V0.4 本地按键触发开关后，使能平台重连(默认30min)                           24-06-16
        心跳包异常时连续发送3次→2次
        验证平台删除、通信、自动注册
   V1.0 正式发布版本                                                        24-06-23
   V1.1 按键循环读取变更为中断向量等待模式，释放部分MCU资源                    24-08-04
        开机后设置禁止休眠
        replaceJson()内部禁止打印，减少后期信息输出数据量
        上电同时发送心跳，携带网络信息
   v1.2 增加AEP平台账号切换宏定义                                            24-08-06
        平台掉线后，重新检测驻网30min→1min，同时搜网Cereg逻辑同步微调
        修改平台去注册逻辑，上电后直接执行→注册失败后执行，解决1min/次重复去注册时平台反应较慢而注册已经结束(才回复去注册信息)
   V1.3 加入4路控制功能                                                     24-08-11
        加入搜网机制设置，网络切换时需按照信号优先规则
        上电延迟2S --> 100mS                                                  
        上电后立即设置IO输出L
        关闭bsp_config.h中枚举RNDIS功能，只使能USB
   V2.0 更新V4.0.0基线                                                     24-09-18
        增加UART输出功能
        增加4路输入、4路输出功能
        增加开机数据中上电后设备开机原因                                     24-09-28
        修复用户上电，然后扫码注册后开机信息丢失问题
        增加串口task初始化后上报01数据
   V2.1 上电延迟500mS --> 1S，实测500mS开机数据无法捕捉                      24-10-10
        修复网络指示功能，区分上电、读卡、网络注册、平台注册状态     
        使能bsp_config.h中AT输出功能
   V2.2 更新V4.0.1 SDK                                                     24-10-28
        增加模组电压、温度检测，心跳包更新上报                               
        增加UART协议中Rcv、Freq溢出异常上报功能
        增加FOTA
   V2.3 增加写Flash增加读取校验与异常写入时上报                              24-11-19
        增加开机后"状态记忆"信息上报
   V2.4 修复SDK切换过程中bsp_config.h未设置"AT"、"RNDIS"问题                 24-12-27
        加入"复位功能"，通过.h宏定义确认是否开启
   V2.5 增加"正反转"产品                                                   
        mv_nvm_config.c中设置上电默认为CFUN0                                25-02-19
   V2.6 修改Task创建方式，修复Fota底层bug                                   25-02-24
        增加平台下发时RmtSwich_workmode_excute函数判断，提升函数健壮性
   V2.7 增加liot_gpio_set_voltage(LIOT_VOL_2_80V)                          25-02-27
   V2.8 增加平台注册等待时间，7.7S-->16.5S                                   25-04-29
        规范版本更新时同步修改modle_config.cfg中的clientVersion，便于后期AT+CGMR指令查询版本   
   V2.8.1 模组复位逻辑1hour-->非ZY变更为24hour                               25-05-11
   V2.8.2 HM磁保持-->继电器输出，100mS-->1S，属于单独临时修改                 25-05-11
   V2.8.4 HM磁保持-->继电器输出，100mS-->1S，属于临时修改，同时删除IMEI检测    25-06-02
   V2.8.5 HS，属于临时修改，删除IMEI检测                                     25-06-03
   V2.9.0 uart_set_io_switch_flag()增加#define定义，只有在开启UART的时候才执行，避免未定义变量赋值    25-06-15
          心跳包中timerDiscon()、UartHeartBeat()增加#define定义，减少内部资源开销
          删除心跳包中"华笙"平台对应"离线定时"相关无效信息，简化后期平台数据显示
          增加按键msgID区分,区分开关不同触发源
          增加全局变量，状态更新时同时赋值该变量，开关数据上报的时优先进行判断开关变量是被异常篡改
   V2.9.1 修改K1_ON/K1_OFF可选时间，增加抗干扰能力进行对比                    25-06-16
   V2.9.2 修改K1_ON/K1_OFF消抖时间50mS-->70mS，增加抗干扰能力进行对比         25-06-16
   V2.9.4 USE_THREE_KEY宏定义拆分出MAG_LATCH_PULSE_OUTPUT_ENABLE_100MS，后期输入、输出独立定义              25-06-16        
          协议后期增加"1_3"、"3_1"产品进行说明
   V2.9.5 正式提供70MS延时3_3固件，FOTA升级尝试解决自动关闭问题                 25-06-19

   -----------------V3版本之后，版本号对应的固件要保证唯一性，相同版本号不能即是1_1又是3_3-----------------------

   V3.0   增加Voltage上传、报警阈值设定、平台主动获取参数功能
          删除switch类产品的心跳包中"workmode"参数，只在开机数据中增加改参数，精简协议
          修复未驻网或平台未注册时UART心跳包被打断问题
          修改UART释放信号量位置，放到每个按键中，解决未驻网或者平台未注册无法释放UART信号量的问题
          UART开关设备修改对应的msgID为"UART_K"，便于后期维护
          屏蔽UART心跳，所有信息同步双方主动执行，同时为MCU发送2S/次数据做预留
          AEP_HS正式删除上电期间IMEI范围校验，后期全部改为线上平台校验
          任意平台下行数据，心跳包计数清0，即3min延迟
          速率正式修改为4800bps
          增加双方主动获取模组开关状态'Q'命令
          UART发送PowerON数据之后，增加IO同步命令，确保MCU不解析PowerON时IO状态可以同步
          增加ModlInfo命令，用于平台无开机信息时可主动获取

          cat1.initSucFlag失效时，串口反馈voltage需返回错误标志！
   V3.1   基于3.0 UART基础上正式输出林工                                   25-07-31


   V3.5   SDK5.04，模组切换NT26KCNB00NNC                                  25-09-22
          底层函数使用liot_network_register_cereg_get替代liot_network_register_get，修复离线查询驻网持续成功bug
          驻网超时加入清频

   V4.0   Motor UART透传  (CQ)                                             25-09-22
   V4.1   修改驻网查询CEREG判断函数  (YMX)
          驻网失败增加清频操作
          modle_config.cfg模组NNC->NNA，适配当前批量客户
          更新UART上电推送内容，增加上电信息与状态推送时间间隔
   V4.2   增加UART HEX通信协议                                             25-11-06
          UART初始化不在推送给MCU信息，解决设备上电开启后模组异常关闭问题
          修改异常交互周期上报10min-->18hour，防止真的MCU异常导致频繁上报
   V4.2.1 上传正反转状态时同步更新电压、电流，故障状态(故障待确认)             25-11-08
   V4.3.0 增加电压波动>10V，电流波动>1A上传                                 25-11-14
          优化平台下发与本地UART心跳冲突问题
   v4.4   正反转中增加SeverDataRead_ModlInfo解析，解决先上电后扫码注册信息显示问题 25-11-15
   V4.5   修复故障后未立即上报问题                                         25-11-17

   V4.6  修复电压、电流波动与正反转动作重复发送问题
         修复平均电流问题，取最新参数
         代码整合


        待更新：24小时，获取时间，同时凌晨上传堆栈空间
               增加IOCheck，IO检测不使用变量，循环监测1S/次，连续10S异常则异常
               replaceJson()针对字符串检测
               
   V6.0.0 架构调整，适配不同模组、不同平台、不同协议(IO/UART)                    26-02-16
        屏蔽回调函数中所有信息打印                                         
        心跳包中增加是否平台下行数据成功信息，使用Key=rev键值对，便于后期问题排查
        心跳包时如果发现没有下行交互，则重新订阅下行topic
        上电发送PowerOn数据后，增加下行topic订阅，即初始化之后订阅2次，修复个别模组订阅失败问题
        新增LinkTest命令，模组匹配并返回"LinkT_Ack"命令
        平台下行命令异常时增加onenet反馈"PlatFBAck"反馈命令(data为Succ/Fail)，该命令用于反馈OneNet防止等待与阻塞
   V6.0.1 UART接收数据时，只有IO状态变化才执行，否则不在执行，解决磁脉冲持续输出问题 26-02-28
   V6.0.2 优化output字段填充                                               26-03-02
          增加AEP功能
   V6.0.4 修复MemState交互问题                                             26-03-07
   V6.0.6 增加MemState中Keep状态，解决适配UART通信中重启状态变更问题            26-03-07
   V6.0.7 增加Switch Key                                                 26-03-08
          如果开启了UART，OUT_IO在UART未反馈时则不在变化
          nenet_datasend_ack已经包含了OneNet的反馈，但是iot_datasend_fault没有，onenet_datasend_ack删除单独增加额外反馈
   V6.0.8 修复OneNet日志不匹配问题                                          26-03-13
   V6.0.9 增加AEP Motor/Motor Pro基本控制                                  26-03-15
          修复 int drv_cmd4_parse_and_update_io(const char *data)中判断逻辑问题，更新为 if (new_status != g_io.out[i].now_status)
          修复 msgID提取溢出问题 extractJson(cat1.rcv.onenet_buf, "msgID", cat1.rcv.SerRandomID, RCV_VALUE_SIZE_10);
          增加Uart Hex兼容
          将UART由向量模式变更为队列形式
   V6.1 SWT_UART9600正式版本                                            26-03-22
   V6.2 增加SWT Hex兼容                                                 26-03-29
        修改V/I发送频率处理逻辑，由等待3min不上报变更为固定3min清0，优化体验
        正反转HEX正式版
   V6.3.0 正反转正式版                                                     26-04-07
   V6.3.1 河马开关3_3正式版                                                26-04-16
          
          常开/常关重复按键需修复发送问题

hex_send_extended_ack - V6.2 Motor待检测！因为V6.0使能而后面屏蔽了

    ModlInfo待测试验证
    通信频率-无心跳
    通信频率-频率过高
    通信频率-上传uartCycle

Motor平台下发待测试


测试记录：
1. 服务器下发"IO1开"：{"cmd":"RmtSwich","data":"0001","msgID":"36E2D5"}
   模组返回：         {"msgID":"36E2D5","data":"0001","cmd":"RemotAck"}
   服务器下发"IO1关"：{"cmd":"RmtSwich","data":"0000","msgID":"8A3207"}
   模组返回：         {"msgID":"8A3207","data":"0000","cmd":"RemotAck"}
     
     1路输出，复测 on/off/pre 功能 ——> 等待测试验证 24-08-11
     1路输出，修改状态记忆后，不需要额外按键，重启立即记忆开关状态 ——> 等待测试验证 24-08-11
     4路输出，返回平台数据 ——> 等待测试验证 24-08-11
     {"msgID":"36E2D5","data":"0000","cmd":"RmtSwich"}
     {"msgID":"36E2D5","data":"0011","cmd":"RmtSwich"}
     {"msgID":"36E2D5","data":"1111","cmd":"RmtSwich"}

     4路输出，修改状态记忆后，不需要额外按键，重启立即记忆开关状态 ——> 等待测试验证 24-08-11
     4路输出，远程控制后 —> 上电读取4路，开、关、保持(16种)，修改完成 ——> 等待测试验证 24-08-11


2. 服务器下发Fota升级：
   {"cmd":"DiffFota","data":"https://cloud.hsiot.ltd/fota/HS_V2.7.1_3_3_V2.9.4_1_3.par","msgID":"123456"}
   {"cmd":"DiffFota","data":"https://cloud.hsiot.ltd/fota/HS_SWITCH_V3.5.0_3_3_V3.5.1_1_3_NNC.par","msgID":"123456"}
   {"cmd":"DiffFota","data":"https://cloud.hsiot.ltd/fota/test_V6.1.1_V6.1.1.par","msgID":"123456"}
   {"cmd":"DiffFota","data":"https://xiot.oss.senthink.com/upload-http-ftp/17EDE081/V6.0.5_V6.0.4.par","msgID":"123456"}



3. 服务器下发状态记忆：
   服务器下发"上电'开'"：{"msgID":"978353","data":"Open","cmd":"MemState"} 
   模组返回：           {"msgID":"978353","data":"Open","cmd":"MemoryAck"}
   服务器下发"上电'关'"：{"msgID":"553322","data":"Clos","cmd":"MemState"}
   模组返回：           {"msgID":"553322","data":"Clos","cmd":"MemoryAck"}
   服务器下发"上电'保持'"：{"msgID":"112233","data":"Hold","cmd":"MemState"}
   模组返回：             {"msgID":"112233","data":"Hold","cmd":"MemoryAck"}


4. 设备主动更新IO开关
   模组接收：AA40001Y9574FF
   模组返回：AAF0001Y9592FF

   模组接收：AA40000Y9573FF
   模组返回：AAF0001Y9591FF


5. 设备主动更新电流、电压参数
   模组接收：AA5V341.0FFFFFF1840FF
   模组返回：AAFV341.0FFFFFF1857FF

   模组接收：AA5V286.0FFFFFF3951FF
   模组返回：AAFV286.0FFFFFF3968FF


6. 服务器下发获取设备信息：
   服务器下发"同步设备信息"：{"msgID":"937353","data":"null","cmd":"ParamGet"}
   
   模组返回：{"vol":"000.0","vbat":"3.3V","temp":"+32C","output":"0000","netSignal":"-080 024","msgID":"937353","cmd":"ParamUpd"}
   模组发送：AAG0000000000001478FF
   模组接收：AA5V380.0FFFFFF39XXFF
   模组发送：AAFV380.0FFFFFF3963FF
   模组返回：{"vol":"380.0","vbat":"3.3V","temp":"+32C","output":"0000","netSignal":"-080 024","msgID":"937353","cmd":"ParamUpd"}


7. 服务器下发设置报警阈值：
   服务器下发"同步设备信息"：{"msgID":"987353","data":"OV:250.0|UV:180.0|OC:FFFFF","cmd":"SetLimit"}
   模组发送：AA6V250.0V180.0AFFFFF0362FF
   模组接收：AA6V250.0V180.0AFFFFF03XXFF
   模组接收：AA6V400.0V200.0AFFFFF0453FF
   模组接收：AA6V400.0V200.0AFFFFF0453FF
   模组接收：AA6V240.0V175.0AFFFFF0668FF
   模组接收：AA6V240.0V175.0AFFFFF0668FF
   {"workmode":"Swt","msgID":"987353","data":"Succ","cmd":"SetLimAck"}
   
   {"workmode":"Swt","msgID":"987353","data":"Fail","cmd":"SetLimAck"}


8. 设备阈值报警
   模组接收：AAWOV265.07290FF
   模组发送：AAWOV265.07238FF

9. 获取模组信息：
   模组接收：{"cmd":"ModlInfo","data":"NULL","msgID":"36E2D5"}
   模组发送：
   {workmode:"Swt",ver:"3.0.5",pwr:"RST",product:"v_1",output:"0000",msgID:"36E2D5",mem:"Clos",iccid:"89861124212030231758",cmd:"ModlInfo"}


Flash读写测试
1. 初次下载固件，初始值写入
   1.1 如果没有写入过，需要赋值初始值，便于后期判断&重新写入 --> √
   1.2 如果写入过，不在写入                               --> √

2. 如果之前是程序复位，更新sysPara.pwrReason为"SFT"，同时清除标志位；
   2.1 上电复位，上传sysPara.pwrReason为"RST"             --> √
   2.2 软件复位，上传sysPara.pwrReason为"SFT"             --> √
   2.2 重新上电复位，上传sysPara.pwrReason为"RST"而不是"SFT" --> √

3. 默认无任何内容 
   3.1 判断是否满足基本几条判断条件  --> √
   3.1 触发重启1次，读取-->写入-->读取校验-->上电测试 --> √
   3.2 触发重启2次，读取-->写入-->读取校验-->上电测试 --> √
   3.3 触发重启3次，读取-->写入-->读取校验-->上电测试 --> √
   3.3 触发重启4次，读取-->写入-->读取校验-->上电测试 --> √


1. 默认无任何内容
   1.1 下发ON，读取-->写入-->读取校验-->上电测试
   1.2 下发OFF，读取-->写入-->读取校验-->上电测试
   1.3 下发PRE，读取-->写入-->读取校验-->上电测试

3. 默认重启已触发1次
   3.1 下发ON，读取-->写入-->读取校验-->上电测试
   3.2 下发OFF，读取-->写入-->读取校验-->上电测试
   3.3 下发PRE，读取-->写入-->读取校验-->上电测试
4. 默认下发ON
   4.1 触发重启1次，读取-->写入-->读取校验-->上电测试
   4.2 触发重启2次，读取-->写入-->读取校验-->上电测试
   4.3 触发重启3次，读取-->写入-->读取校验-->上电测试
   4.4 触发重启4次，读取-->写入-->读取校验-->上电测试
5. 默认下发PRE
   5.1 触发重启1次，读取-->写入-->读取校验-->上电测试
   5.2 触发重启2次，读取-->写入-->读取校验-->上电测试
   5.3 触发重启3次，读取-->写入-->读取校验-->上电测试
   5.4 触发重启4次，读取-->写入-->读取校验-->上电测试

