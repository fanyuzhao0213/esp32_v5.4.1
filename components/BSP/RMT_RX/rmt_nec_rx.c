/**
 ****************************************************************************************************
 * @file        rmt_nec_rx.c
 ****************************************************************************************************
 */

#include "rmt_nec_rx.h"


/* 保存NEC解码的地址和命令字节 */
uint16_t s_nec_code_address = 0x0000;
uint16_t s_nec_code_command = 0x0000;

const char* RMTTAG = "RMT_RX";

QueueHandle_t receive_queue = NULL;
rmt_channel_handle_t rx_channel = NULL;
rmt_symbol_word_t raw_symbols[64];      /* 对于标准NEC框架应该足够 */
rmt_receive_config_t receive_config;

#define MAX_NEC_SYMBOLS 68  // 最多解析这么多个符号（NEC 标准编码 = 34 组）

/**
 * @brief       RMT数据接收完成回调函数
 * @param       channel   : 通道
 * @param       edata     : 接收的数据
 * @param       user_data : 传入的参数
 * @retval      返回是否唤醒了任何任务
 */
bool rmt_nec_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;

    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);     /* 将收到的RMT数据通过消息队列发送到解析任务 */

    return high_task_wakeup == pdTRUE;
}

/**
 * @brief       RMT红外接收初始化
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t rmt_nec_rx_init(void)
{
    ESP_ERROR_CHECK(gpio_reset_pin(RMT_IN_GPIO_PIN));
    /* 配置接收通道 */
    rmt_rx_channel_config_t rx_channel_cfg = {
        .gpio_num           = RMT_IN_GPIO_PIN,          /* 设置红外接收通道管脚 */
        .clk_src            = RMT_CLK_SRC_DEFAULT,      /* 设置RMT时钟源 */
        .resolution_hz      = RMT_RESOLUTION_HZ,        /* 设置时钟分辨率 */
        .mem_block_symbols  = 64,                       /* 通道一次可以存储的RMT符号数量 */
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));  /* 创建接收通道 */

    /* 配置红外接收完成回调 */
    receive_queue = xQueueCreate(2, sizeof(rmt_rx_done_event_data_t));  /* 创建消息队列，用于接收红外编码 */
    assert(receive_queue);
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_nec_rx_done_callback,                       /* RMT信号接收完成回调函数 */
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));              /* 配置RMT接收通道回调函数 */

    /* NEC协议的时序要求 */
    receive_config.signal_range_min_ns = 1250;          /* NEC信号的最短持续时间为560us，1250ns＜560us，有效信号不会被视为噪声 */
    receive_config.signal_range_max_ns = 12000000;      /* NEC信号的最长持续时间为9000us，12000000ns>9000us，接收不会提前停止 */

    /* 开启RMT通道 */
    ESP_ERROR_CHECK(rmt_enable(rx_channel));            /* 使能RMT接收通道 */
    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));    /* 准备接收 */

    return ESP_OK;
}

/**
 * @brief       判断数据时序长度是否在NEC时序时长容差范围内 正负REMOTE_NEC_DECODE_MARGIN的值以内
 * @param       signal_duration:信号持续时间
 * @param       spec_duration:信号的标准持续时间
 * @retval      true:符合条件;false:不符合条件
 */
inline bool rmt_nec_check_range(uint32_t signal_duration, uint32_t spec_duration)
{
    return (signal_duration < (spec_duration + RMT_NEC_DECODE_MARGIN)) &&
           (signal_duration > (spec_duration - RMT_NEC_DECODE_MARGIN));
}

/**
 * @brief       对比数据时序长度判断是否为逻辑0
 * @param       rmt_nec_symbols:RMT数据帧
 * @retval      true:符合条件;false:不符合条件
 */
bool rmt_nec_logic0(rmt_symbol_word_t *rmt_nec_symbols)
{
    return rmt_nec_check_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ZERO_DURATION_0) &&
           rmt_nec_check_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ZERO_DURATION_1);
}

/**
 * @brief       对比数据时序长度判断是否为逻辑1
 * @param       rmt_nec_symbols:RMT数据帧
 * @retval      true:符合条件;false:不符合条件
 */
bool rmt_nec_logic1(rmt_symbol_word_t *rmt_nec_symbols)
{
    return rmt_nec_check_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ONE_DURATION_0) &&
           rmt_nec_check_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ONE_DURATION_1);
}

/**
 * @brief       将RMT接收结果解码出NEC地址和命令
 * @param       rmt_nec_symbols:RMT数据帧
 * @retval      true成功;false失败
 */
bool rmt_nec_parse_frame(rmt_symbol_word_t *rmt_nec_symbols)
{
    rmt_symbol_word_t *cur = rmt_nec_symbols;
    uint16_t address = 0;
    uint16_t command = 0;

    ESP_LOGI("RMT_NEC", "接收到 RMT 符号（最多打印前 %d 个）：", MAX_NEC_SYMBOLS);
    for (int i = 0; i < MAX_NEC_SYMBOLS; i++)
    {
        if (cur[i].duration0 == 0 && cur[i].duration1 == 0)
            break;  // 遇到空数据就停止

        ESP_LOGI("RMT_NEC", "[%02d] level0: %5dus | level1: %5dus", i, cur[i].duration0, cur[i].duration1);
    }

    // 校验前导码
    if (!(rmt_nec_check_range(cur[0].duration0, NEC_LEADING_CODE_DURATION_0) &&
          rmt_nec_check_range(cur[0].duration1, NEC_LEADING_CODE_DURATION_1)))
    {
        ESP_LOGW("RMT_NEC", "前导码不正确");
        return false;
    }

    cur++;  // 跳过引导码

    // 解析地址
    for (int i = 0; i < 16; i++)
    {
        if (rmt_nec_logic1(cur))
            address |= 1 << i;
        else if (rmt_nec_logic0(cur))
            address &= ~(1 << i);
        else
        {
            ESP_LOGW("RMT_NEC", "地址第 %d 位无效", i);
            return false;
        }
        cur++;
    }

    // 解析命令
    for (int i = 0; i < 16; i++)
    {
        if (rmt_nec_logic1(cur))
            command |= 1 << i;
        else if (rmt_nec_logic0(cur))
            command &= ~(1 << i);
        else
        {
            ESP_LOGW("RMT_NEC", "命令第 %d 位无效", i);
            return false;
        }
        cur++;
    }

    s_nec_code_address = address;
    s_nec_code_command = command;

    ESP_LOGI("RMT_NEC", "NEC解码成功：地址=0x%04X, 命令=0x%04X", address, command);
    return true;
}

/**
 * @brief       检查数据帧是否为重复按键：一直按住同一个键
 * @param       rmt_nec_symbols:RMT数据帧
 * @retval      true:符合条件;false:不符合条件
 */
bool rmt_nec_parse_frame_repeat(rmt_symbol_word_t *rmt_nec_symbols)
{
    return rmt_nec_check_range(rmt_nec_symbols->duration0, NEC_REPEAT_CODE_DURATION_0) &&
           rmt_nec_check_range(rmt_nec_symbols->duration1, NEC_REPEAT_CODE_DURATION_1);
}



/**
 * @brief       根据NEC编码解析红外协议并打印指令结果
 * @param       rmt_nec_symbols : 数据帧
 * @param       symbol_num      : 数据帧大小
 * @retval      无
 */
void rmt_rx_scan(rmt_symbol_word_t *rmt_nec_symbols, size_t symbol_num)
{
    uint8_t rmt_data = 0;
    uint8_t tbuf[40];
    char *str = 0;

    switch (symbol_num)         /* 解码RMT接收数据 */
    {
        case 34:                /* 正常NEC数据帧 */
        {
            if (rmt_nec_parse_frame(rmt_nec_symbols) )
            {
                rmt_data = (s_nec_code_command >> 8);

                switch (rmt_data)
                {
                    case 0xBA:
                    {
                        str = "POWER";
                        break;
                    }

                    case 0xB9:
                    {
                        str = "UP";
                        break;
                    }

                    case 0xB8:
                    {
                        str = "ALIENTEK";
                        break;
                    }

                    case 0xBB:
                    {
                        str = "BACK";
                        break;
                    }

                    case 0xBF:
                    {
                        str = "PLAY/PAUSE";
                        break;
                    }

                    case 0xBC:
                    {
                        str = "FORWARD";
                        break;
                    }

                    case 0xF8:
                    {
                        str = "vol-";
                        break;
                    }

                    case 0xEA:
                    {
                        str = "DOWN";
                        break;
                    }

                    case 0xF6:
                    {
                        str = "VOL+";
                        break;
                    }

                    case 0xE9:
                    {
                        str = "1";
                        break;
                    }

                    case 0xE6:
                    {
                        str = "2";
                        break;
                    }

                    case 0xF2:
                    {
                        str = "3";
                        break;
                    }

                    case 0xF3:
                    {
                        str = "4";
                        break;
                    }

                    case 0xE7:
                    {
                        str = "5";
                        break;
                    }

                    case 0xA1:
                    {
                        str = "6";
                        break;
                    }

                    case 0xF7:
                    {
                        str = "7";
                        break;
                    }

                    case 0xE3:
                    {
                        str = "8";
                        break;
                    }

                    case 0xA5:
                    {
                        str = "9";
                        break;
                    }

                    case 0xBD:
                    {
                        str = "0";
                        break;
                    }

                    case 0xB5:
                    {
                        str = "DELETE";
                        break;
                    }

                }
                ESP_LOGI(RMTTAG, "KEYVAL = %d, Command=%04X", rmt_data, s_nec_code_command);
            }
            break;
        }
        case 2:     /* 重复NEC数据帧 */
        {
            if (rmt_nec_parse_frame_repeat(rmt_nec_symbols))
            {
                ESP_LOGI(RMTTAG,"KEYVAL = %d, Command = %04X, repeat", rmt_data, s_nec_code_command);
            }
            break;
        }
        default:    /* 未知NEC数据帧 */
        {
            ESP_LOGI(RMTTAG, "Unknown NEC frame");
            break;
        }
    }
}

void rmt_rx_task(void *pvParameters)
{
    rmt_rx_done_event_data_t rx_data;           //专用结构数据来接收nec数据
    while (1)
    {
        if (xQueueReceive(receive_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            rmt_rx_scan(rx_data.received_symbols, rx_data.num_symbols);                                     /* 解析接收符号并打印结果 */
            ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));    /* 重新开始接收 */
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
