#include "pwm.h"
#include "esp_log.h"

static const char *TAG = "PWM";

esp_err_t pwm_init(uint32_t freq_hz, uint32_t duty_percent)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode       = PWM_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,  // 分辨率 13 位 (0-8191)
        .timer_num        = PWM_TIMER,
        .freq_hz          = freq_hz,
        .clk_cfg          = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t channel_conf = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_GPIO,
        .duty           = 0,  // 初始占空比 0
        .hpoint         = 0
    };

    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return pwm_set_duty(duty_percent);
}

esp_err_t pwm_set_duty(uint32_t duty_percent)
{
    if (duty_percent > 100) {
        duty_percent = 100;
    }

    uint32_t max_duty = (1 << 13) - 1;  // 13位分辨率最大值：8191
    uint32_t duty = (max_duty * duty_percent) / 100;

    esp_err_t ret = ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(PWM_MODE, PWM_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pwm_stop(void)
{
    esp_err_t ret = ledc_stop(PWM_MODE, PWM_CHANNEL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_stop failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
