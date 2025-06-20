/**
 ****************************************************************************************************
 * @file        adc1.h
 ****************************************************************************************************
 */

#ifndef __ADC_H
#define __ADC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"


/* 管脚声明 */
/*  ESP32-S3 有两个 ADC 模块：
    ADC1：支持 8 个通道（GPIO1、GPIO2、GPIO3、GPIO4、GPIO5、GPIO6、GPIO7、GPIO8
    ADC2：支持 10 个通道（GPIO9 ~ GPIO20）
    每个 ADC 的通道号固定映射到对应 GPIO，不能随意换
*/
#define ADC_CHAN    ADC_CHANNEL_7       /* 对应管脚为GPIO8 */

/* 函数声明 */
void adc_init(void);                                               /* 初始化ADC */
uint32_t adc_get_result_average(adc_channel_t ch, uint32_t times); /* 获取ADC转换且进行均值滤波后的结果 */
void adc_task(void *pvParameters);
#endif
