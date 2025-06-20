/**
 ****************************************************************************************************
 * @file        rmt_nec_tx.h
 ****************************************************************************************************
 */

#ifndef __RMT_NEC_TX_H
#define __RMT_NEC_TX_H

#include "driver/rmt_tx.h"
#include "ir_nec_encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

/* 引脚定义 */
#define RMT_TX_PIN                  GPIO_NUM_8      /* 连接RMT_TX_IN的GPIO端口 */
#define RMT_TX_HZ                   1000000         /* 1MHz 频率, 1 tick = 1us */

/* 外部调用 */
extern rmt_encoder_handle_t nec_encoder;
extern rmt_transmit_config_t transmit_config;
extern rmt_channel_handle_t tx_channel;

/* 函数声明 */
esp_err_t rmt_nec_tx_init(void);
void rmt_tx_task(void *pvParameters);
/*发射特定红外码*/
void rmt_send_nec(uint16_t addr, uint16_t cmd);
#endif
