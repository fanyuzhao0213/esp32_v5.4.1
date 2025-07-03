#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_psram.h"
#include "esp_flash.h"
#include <stdio.h>
#include "esp_log.h"

#include "record.h"
#include "http.h"
#include "wifi.h"
#include "led.h"
#include "exit.h"
#include "esptimer.h"
#include "timg.h"
#include "pwm.h"
#include "myiic.h"
#include "xl9555.h"
#include "at24c02.h"
#include "ap3216c.h"
#include "adc.h"
#include "rmt_nec_rx.h"
#include "rmt_nec_tx.h"
#include "my_spiffs.h"
#include "my_spi.h"
#include "spi_sd.h"
#include "sdmmc_cmd.h"
#include "../components/Middlewares/MYFATFS/exfuns.h"
#include "audioplay.h"
#include "mp3_decoder.h"

#define TAG "MAIN"

static void ledc_init_example(void);
static void timer_init_example(void);
static void printf_chip_info(void);
static void my_eeprom_init(void);
static void my_hardware_init(void);
esp_err_t my_mp3_play(char* path);
/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();     /* 初始化NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    printf_chip_info();             //打印板载信息
    ESP_ERROR_CHECK(spiffs_init("storage", DEFAULT_MOUNT_POINT, DEFAULT_FD_NUM));    /* SPIFFS初始化 */
    spiffs_test();


    my_wifi_init();
    xTaskCreate(http_get_task, "http_get_task", 8192, NULL, 5, NULL);
    xTaskCreate(decode_mp3_task, "print_task", 4096, NULL, 4, NULL);
    my_hardware_init();             //初始化板级设备信息
    // wav_play_song("0:/MUSIC/2.wav");      //单独播放某一个特定文件的音乐  wav格式

    // const char *mp3_path = "/spiffs/test.mp3"; // 请确保路径正确且已挂载
    // const char *mp3_path = "/0:/MP3/renjianyanhuo.mp3"; // 请确保路径正确且已挂载
    // my_mp3_play("/0:/MP3/renjianyanhuo.mp3");
    my_recorder_init();
    while(1) {
        // audio_play();       /* 循环播放音乐 */
        vTaskDelay(pdMS_TO_TICKS(10)); /* 延时 */
    }
}

esp_err_t my_mp3_play(char* path)
{
    esp_err_t ret;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file %s", path);
        return -1;
    }
    ret = play_mp3_file(fp);

    return ret;
}

static void my_hardware_init(void)
{
    esp_err_t ret;

    size_t bytes_total, bytes_free;         /* SD卡的总空间与剩余空间 */
    /* 初始化基础外设 */
    //led_init();                           /* 初始化LED */
    pwm_init(1000, 0);                      // 1kHz, 50% 占空比
    pwm_set_duty(100);                      //设置占空比为0   最大亮度  设置为100 则灭
    exit_init();                            /* 初始化按键 */
    esptimer_init(1000000);                 /* 初始化高分辨率定时器 (1秒周期) */
    once_timer_init(5000000);               /* 初始化单次定时器 (5秒周期) */
    myiic_init();                           /* 初始化IIC0 */
    xl9555_init();                          /* 初始化XL9555 */
    my_eeprom_init();                       /* 初始化 eeprom */
    adc_init();                             /* 初始化 ADC */
    ap3216c_init();                         /* 初始化 ap3216C */
    rmt_nec_rx_init();                      /* RMT 红外接收器件初始化*/
    rmt_nec_tx_init();                      /* RMT 红外发送器件初始化 */
    my_spi_init();                          /* SPI初始化 */

    while (sd_spi_init())       /* 检测不到SD卡 */
    {
        ESP_LOGE(TAG,"SD Card Error!");
        vTaskDelay(500);
        ESP_LOGE(TAG, "Please Check! ");
        vTaskDelay(500);
    }
    ESP_LOGI(TAG,"SD card init success!");
    sd_get_fatfs_usage(&bytes_total, &bytes_free);
    ret = exfuns_init();    /* 为fatfs相关变量申请内存 */

    while (es8388_init())       /* ES8388初始化 */
    {
        ESP_LOGE(TAG, "ES8388 Error");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG,"ES8388 init success!");
    // 创建 ADC 采集任务
    xTaskCreate(adc_task, "adc_task", 2048, NULL, 5, NULL);
    // 创建 ap3216c 采集任务
    xTaskCreate(ap3216c_task, "ap3216c_task", 2048, NULL, 5, NULL);
    // 创建 rmtrx 采集任务
    xTaskCreate(rmt_rx_task, "rmt_rx_task", 8192, NULL, 5, NULL);
    // 创建 rmttx 发送任务
    //xTaskCreate(rmt_tx_task, "rmt_tx_task", 4096, NULL, 5, NULL);
    // /* 初始化定时器 */
    // timer_init_example();
}
static void my_eeprom_init(void)
{
    esp_err_t ret;
    ret = at24c02_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C init failed! ret=%d", ret);
        return;
    }

    // 调用 AT24C02 检查
    ret = at24c02_check();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "AT24C02 init check failed, please check hardware connection.");
    }
    else
    {
        ESP_LOGI(TAG, "AT24C02 is ready for use.");
    }

    my_iic_eeprom_test();
}
static void printf_chip_info(void)
{
    esp_chip_info_t chip_info; /* 定义芯片信息结构体变量 */
    uint32_t flash_size;
    /* 获取芯片信息并打印 */
    esp_chip_info(&chip_info);
    ESP_ERROR_CHECK(esp_flash_get_size(NULL, &flash_size));

    ESP_LOGI(TAG, "| %-12s | %-10s |", "describe", "explain");
    ESP_LOGI(TAG, "|--------------|------------|");
    if (chip_info.model == CHIP_ESP32S3) {
        ESP_LOGI(TAG, "| %-12s | %-10s |", "model", "ESP32S3");
    }
    ESP_LOGI(TAG, "| %-12s | %-d          |", "cores",      chip_info.cores);
    ESP_LOGI(TAG, "| %-12s | %-d          |", "revision",   chip_info.revision);
    ESP_LOGI(TAG, "| %-12s | %-ld         |", "FLASH size", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "| %-12s | %-2d         |", "PSRAM size", esp_psram_get_size() / (1024 * 1024));
    ESP_LOGI(TAG, "|--------------|------------|");
}
/**
 * @brief       定时器初始化封装函数
 * @param       无
 * @retval      无
 */
static void timer_init_example(void)
{
    timg_config_t *timg_config = malloc(sizeof(timg_config_t));
    if (!timg_config) {
        ESP_LOGE(TAG, "Failed to allocate timer config memory");
        return;
    }

    /* 定时器配置 */
    timg_config->timer_count_value = 0;
    timg_config->clk_src           = GPTIMER_CLK_SRC_DEFAULT;
    timg_config->timer_group       = TIMER_GROUP_0;
    timg_config->timer_idx         = TIMER_0;
    timg_config->timing_time       = 1 * 1000000;  /* 1秒定时 */
    timg_config->alarm_value       = timg_config->timing_time;
    timg_config->auto_reload       = TIMER_ALARM_EN;

    /* 初始化定时器 */
    timg_init(timg_config);

    /* 注意：实际项目中应考虑内存释放时机 */
}




uint16_t decoded[1024] = { 0 };   // 存放解码后的红外数据
/**
 * @brief       填充RMT item电平和持续时间
 * @param[out]  item     : RMT item指针
 * @param[in]   high_us  : 高电平时间 (us)
 * @param[in]   low_us   : 低电平时间 (us)
 */
static inline void nec_fill_item_level(rmt_item32_t *item, int high_us, int low_us)
{
    item->level0 = 1;
    item->duration0 = (high_us) / 10 * 1000000;
    item->level1 = 0;
    item->duration1 = (low_us) / 10 * 1000000;
}


/**
 * @brief       构建RMT item数组
 * @param[out]  item     : 输出RMT item数组
 * @param[in]   item_num : item数组的数量
 */
static void build_tv_rmt_items(rmt_item32_t *item, size_t item_num)
{
    nec_fill_item_level(item, decoded[0], decoded[1]);
    for (size_t i = 1; i < item_num; i++)
    {
        item++;
        nec_fill_item_level(item, decoded[2 * i], decoded[2 * i + 1]);
    }
}


/**
 * @brief       发送TV红外信号
 * @param       key_val : 需要发送的TV按键值（例如 TV_POWER）
 */
void tv_ir_send_example(t_tv_key_value key_val)
{
    char *filepath = "/spiffs/irda_tv_skyworth.bin";

    if (ir_file_open(2, 1, filepath) != 0)
    {
        ESP_LOGE("TV_IR", "打开红外库文件失败: %s", filepath);
        return;
    }

    uint16_t decode_len = ir_decode(key_val, decoded, NULL, 0);
    ir_close();

    if (decode_len == 0)
    {
        ESP_LOGE("TV_IR", "解码按键失败: %d", key_val);
        return;
    }

    if (decode_len > 200)
    {
        decode_len = (decode_len + 1) / 2;
    }

    size_t item_num = decode_len / 2;
    rmt_item32_t *item = (rmt_item32_t *)malloc(sizeof(rmt_item32_t) * item_num);
    if (item == NULL)
    {
        ESP_LOGE("TV_IR", "内存分配失败");
        return;
    }

    build_tv_rmt_items(item, item_num);

    // 发送红外信号
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, item, item_num * sizeof(rmt_item32_t), &transmit_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, pdMS_TO_TICKS(1000)));

    free(item);
}
