#ifndef __PWM_H__
#define __PWM_H__

#include "driver/ledc.h"
#include "esp_err.h"

#define PWM_GPIO            GPIO_NUM_1          // GPIO1
#define PWM_CHANNEL         LEDC_CHANNEL_0      // 通道0
#define PWM_TIMER           LEDC_TIMER_0        // 定时器0
#define PWM_MODE            LEDC_LOW_SPEED_MODE // 低速模式


// 初始化 PWM，设置频率和初始占空比（0-100）
esp_err_t pwm_init(uint32_t freq_hz, uint32_t duty_percent);

// 设置占空比（0-100）
esp_err_t pwm_set_duty(uint32_t duty_percent);

// 停止 PWM 输出
esp_err_t pwm_stop(void);

#endif // __PWM_H__

