

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


const char* TAG = "MAIN";

static void ledc_init_example(void);
static void timer_init_example(void);
static void printf_chip_info(void);
static void my_eeprom_init(void);
static void my_hardware_init(void);
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
    printf_chip_info();

    my_hardware_init();


    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100)); /* 延时 */
    }
}


static void my_hardware_init(void)
{
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
    rmt_nec_rx_init();                      /* RMT 接收初始化*/



    // 创建 ADC 采集任务
    xTaskCreate(adc_task, "adc_task", 2048, NULL, 5, NULL);
    // 创建 ap3216c 采集任务
    xTaskCreate(ap3216c_task, "ap3216c_task", 2048, NULL, 5, NULL);
    // 创建 rmtrx 采集任务
    xTaskCreate(rmt_rx_task, "rmt_rx_task", 4096, NULL, 5, NULL);
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
