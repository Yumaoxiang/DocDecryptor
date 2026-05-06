#include "hal_project.h"


/*******************************************************************************
 * 名    称：adc_vbat_get
 * 功    能: 采样模组vbat电压，同时数值赋值给cat1.vbat
 * 说    明: 模组采样电压如3300，则实际数值为3.3V，取前2位转换为字符串并赋值给cat.vbat
 *******************************************************************************/
void adc_vbat_get() {
    int vbat = 0;
    liot_adc_errcode_e ret;

    ret = liot_adc_get_volt(LIOT_ADC_VBAT_CHANNEL, &vbat);
    liot_trace("adc_vbat_get() vbat %d, ret : %d\r\n", vbat, ret);

    if (ret != LIOT_ADC_SUCCESS || vbat < 2000 || vbat > 5999) {
        strcpy(cat1.vbat, "errV");
        return;
    }

    // 提取数字部分
    int integerPart = vbat / 1000; // 取整部分
    int decimalPart = (vbat % 1000) / 100; // 小数部分

    // 确保整数字符在合理范围内
    if (integerPart < 2 || integerPart > 5 || decimalPart < 0 || decimalPart > 9) {
        strcpy(cat1.vbat, "errV");
        return;
    }

    // 格式化字符串
    snprintf(cat1.vbat, sizeof(cat1.vbat), "%d.%dV", integerPart, decimalPart);
}

/*******************************************************************************
 * 名    称：adc_temp_get
 * 功    能: 采样模组芯片温度，同时数值赋值给cat1.temp
 * 说    明: 模组采样温度如445，则实际数值为44.5℃，取前2位转换为字符串并赋值给cat.temp
 *******************************************************************************/
void adc_temp_get() {
    int temp = 0;
    liot_adc_errcode_e ret;

    ret = liot_adc_get_volt(LIOT_ADC_THERMAL_CHANNEL, &temp);
    liot_trace("LIOT_ADC_THERMAL_CHANNEL thermal %d, ret : %d\r\n", temp, ret);

    if (ret != LIOT_ADC_SUCCESS || temp < -45 || temp > 90) {
        strcpy(cat1.temp, "errC");
        return;
    }

    // 处理符号和提取数字部分
    char sign = temp >= 0 ? '+' : '-';
    temp = abs(temp);

    int integerPart = temp / 10;        // 取整部分
    int decimalPart = (temp % 10);      // 小数部分

    // 格式化字符串
    snprintf(cat1.temp, sizeof(cat1.temp), "%c%d%dC", sign, integerPart, decimalPart);
}

void adc_vbat_temp_get(){
    adc_vbat_get();
    liot_rtos_task_sleep_ms(10);    // 不确定连续采样是否有问题，加入延时
    adc_temp_get();
}