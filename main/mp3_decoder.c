#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "audio_player.h"
#include "es8388.h"
#include <errno.h>
#include "ff.h"  // 添加 FatFs 头文件

#include "mp3_decoder.h"

static const char *TAG = "MP3_ES8388";

/* I2S GPIOs from your schematic */
#define I2S_MCLK    3
#define I2S_BCLK    46
#define I2S_LRCK    9
#define I2S_DOUT    14
#define I2S_DIN     10

// #define SPK_EN_IO   46

/* I2S 写函数 */
static esp_err_t my_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms) {
    return i2s_channel_write(tx_handle, audio_buffer, len, bytes_written, timeout_ms);
}

/* I2S 时钟重配置 */
static esp_err_t my_i2s_reconfig_clk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch) {
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(bits_cfg, ch),
        .gpio_cfg = {
            .mclk = I2S_MCLK,
            .bclk = I2S_BCLK,
            .ws   = I2S_LRCK,
            .dout = I2S_DOUT,
            .din  = I2S_DIN,
        },
    };
    i2s_channel_disable(tx_handle);
    i2s_channel_reconfig_std_clock(tx_handle, &cfg.clk_cfg);
    i2s_channel_reconfig_std_slot(tx_handle, &cfg.slot_cfg);
    return i2s_channel_enable(tx_handle);
}

/* 静音控制 */
static esp_err_t audio_mute(AUDIO_PLAYER_MUTE_SETTING setting) {
    // gpio_set_level(SPK_EN_IO, setting == AUDIO_PLAYER_MUTE ? 0 : 1);
    //静音控制
    /* 关闭喇叭 */
    xl9555_pin_write(SPK_EN_IO, 1);
    return ESP_OK;
}

void list_mp3_files()
{
    FATFS fs;
    FRESULT res;
    FF_DIR  dir;
    FILINFO fno;

    // 这里假设你已经挂载好了 FATFS，且盘符为 0:
    // 如果未挂载请先调用 esp_vfs_fat_sdspi_mount()

    // 打开目录 0:/MP3
    res = f_opendir(&dir, "0:/MP3");
    if (res != FR_OK) {
        ESP_LOGE(TAG, "Failed to open directory 0:/MP3, error: %d", res);
        return;
    }

    ESP_LOGI(TAG, "Listing files in 0:/MP3 ...");

    // 读取目录项
    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) {
            // 读完或出错
            break;
        }

        if (fno.fattrib & AM_DIR) {
            ESP_LOGI(TAG, "[DIR]  %s", fno.fname);
        } else {
            ESP_LOGI(TAG, "[FILE] %s  size: %lu bytes", fno.fname, fno.fsize);
        }
    }

    f_closedir(&dir);
}

static QueueHandle_t event_queue; // 用于接收播放器事件

// 播放器回调，用于接收播放状态事件static void audio_player_event_cb(audio_player_cb_ctx_t *ctx)
static void audio_player_event_cb(audio_player_cb_ctx_t *ctx)
{
    if (!ctx) return;

    ESP_LOGI(TAG, "Audio event: %d", ctx->audio_event);

    if (event_queue) {
        xQueueSend(event_queue, &(ctx->audio_event), 0);
    }

    switch (ctx->audio_event) {
        case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
            ESP_LOGI(TAG, "🎵 正在播放");
            break;
        case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
            ESP_LOGI(TAG, "⏸️ 暂停播放");
            break;
        case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
            ESP_LOGI(TAG, "✅ 播放结束或停止");
            break;
        case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN:
            ESP_LOGE(TAG, "❌ 播放出错！");
            break;
        case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
            ESP_LOGE(TAG, "❌ 播放结束！");
            break;
        default:
            break;
    }
}




// 播放MP3文件
esp_err_t play_mp3_file(FILE *fp)
{
    if (!fp) return ESP_ERR_INVALID_ARG;

    myi2s_init();                                   /* I2S初始化 */
    es8388_adda_cfg(1,0);                           /* 打开DAC，关闭ADC */
    es8388_input_cfg(0);                            /* 录音关闭 */
    es8388_output_cfg(1,1);                         /* 喇叭通道和耳机通道打开 */
    es8388_hpvol_set(10);                           /* 设置耳机 */
    es8388_spkvol_set(0);                           /* 设置喇叭 */
    xl9555_pin_write(SPK_EN_IO,0);                  /* 打开喇叭 */

    audio_player_config_t config = {
        .mute_fn = audio_mute,
        .write_fn = my_i2s_write,
        .clk_set_fn = my_i2s_reconfig_clk,
        .priority = 5,
        .coreID = 0,
    };

    esp_err_t ret = audio_player_new(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_new failed: %s", esp_err_to_name(ret));
        return ret;
    }

    event_queue = xQueueCreate(5, sizeof(audio_player_callback_event_t));
    if (!event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        audio_player_delete();
        return ESP_FAIL;
    }

    audio_player_callback_register(audio_player_event_cb, NULL);

    ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        vQueueDelete(event_queue);
        audio_player_delete();
        return ret;
    }

    // 等待播放完成（收到IDLE事件）
    audio_player_callback_event_t evt;
    while (1) {
        if (xQueueReceive(event_queue, &evt, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (evt == AUDIO_PLAYER_CALLBACK_EVENT_PAUSE) {
                ESP_LOGI(TAG, "Playback finished");
                break;
            }
        }
    }

    vQueueDelete(event_queue);
    audio_player_delete();
    vTaskDelay(pdMS_TO_TICKS(100));  // ⏱️给播放器内部线程一点时间释放文件
    audio_stop();                   /* 先停止播放 */
    i2s_deinit();                   /* 卸载I2S */
    fclose(fp);  // 🔐 播放结束后安全关闭文件
    return ESP_OK;
}

