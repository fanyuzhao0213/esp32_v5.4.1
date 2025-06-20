/**
 ****************************************************************************************************
 * @file        rmt_nec_tx.c
 ****************************************************************************************************
 */

#include "rmt_nec_tx.h"


const char* RMTTX_TAG = "RMT_TX";

rmt_encoder_handle_t nec_encoder;
rmt_transmit_config_t transmit_config;
rmt_channel_handle_t tx_channel;

/**
 * @brief       RMT红外发送初始化
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t rmt_nec_tx_init(void)
{
    /* 配置发送通道 */
    rmt_tx_channel_config_t tx_channel_cfg = {
        .gpio_num           = RMT_TX_PIN,               /* RMT发送通道引脚 */
        .clk_src            = RMT_CLK_SRC_DEFAULT,      /* RMT发送通道时钟源 */
        .resolution_hz      = RMT_TX_HZ,                /* RMT发送通道时钟分辨率 */
        .mem_block_symbols  = 64,                       /* 通道一次可以存储的RMT符号数量 */
        .trans_queue_depth  = 4,                        /* 允许在后台挂起的事务数，本例不会对多个事务进行排队，因此队列深度>1就足够了 */
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));  /* 创建一个RMT发送通道 */

    /* 配置载波与占空比 */
    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = 38000,                                          /* 载波频率，0表示禁用载波 */
        .duty_cycle = 0.33,                                             /* 载波占空比33% */
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));       /* 对发送信道应用调制功能 */

    /* 不会在循环中发送NEC帧 */
    transmit_config.loop_count = 0;                                     /* 0为不循环，-1为无限循环 */

    /* 配置编码器 */
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = RMT_TX_HZ,                                        /* 编码器分辨率 */
    };
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));    /* 配置编码器 */

    ESP_ERROR_CHECK(rmt_enable(tx_channel));                            /* 使能发送通道 */

    return ESP_OK;
}

/*
发射特定红外码
*/
void rmt_send_nec(uint16_t addr, uint16_t cmd)
{
    ir_nec_scan_code_t scan_code = {
        .address = addr,
        .command = cmd,
    };
    ESP_LOGI("RMT_TX", "Sending NEC code: addr=0x%04X cmd=0x%04X", addr, cmd);
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, &scan_code, sizeof(scan_code), &transmit_config));
}

void rmt_tx_task(void *pvParameters)
{
    // 常用 NEC 红外码（例如地址 0x00FF，命令 0x00FF）
    ir_nec_scan_code_t scan_code = {
        .address = 0x00FF,
        .command = 0x00FF,
    };

    while (1)
    {
        ESP_LOGI(RMTTX_TAG, "Sending NEC IR code: addr=0x%04X, cmd=0x%04X",
                 scan_code.address, scan_code.command);

        esp_err_t ret = rmt_transmit(tx_channel, nec_encoder, &scan_code, sizeof(scan_code), &transmit_config);

        if (ret == ESP_OK)
        {
            ESP_LOGI(RMTTX_TAG, "Transmit success");
        }
        else
        {
            ESP_LOGE(RMTTX_TAG, "Transmit failed, ret = %d", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));  // 每10秒发一次
    }
}
