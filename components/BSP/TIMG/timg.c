/**
 ****************************************************************************************************
 * @file        timg.c
 */

#include "timg.h"
#include "led.h"
#include "esp_log.h"


/**
 * @brief       定时器组回调函数（常用）
 * @param       args: 传入参数
 * @retval      默认返回1
 */
static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    timg_config_t *user_data = (timg_config_t *) args;
    /* 存储当前计数值变量清零 */
    user_data->timer_count_value = 0;
    /* 获取当前计数值 */
    user_data->timer_count_value = timer_group_get_counter_value_in_isr(user_data->timer_group, user_data->timer_idx);
    LED0_TOGGLE();
    // ESP_LOGI("1", "systerm is running!");
    /* 如果没有开启重装载，那么想要周期警报，必须设置警报值为t0 += t0 */
    if (!user_data->auto_reload)
    {
        user_data->alarm_value += user_data->timing_time;
        /* 设置警报值（ISR类型函数） */
        timer_group_set_alarm_value_in_isr(user_data->timer_group, user_data->timer_idx, user_data->alarm_value);
    }

    return 1;
}

/**
 * @brief       初始化定时器组
 * @param       timg_config: 定时器配置结构体
 * @retval      无
 */
void timg_init(timg_config_t *timg_config)
{
    uint32_t clk_src_hz = 0;
    timer_config_t timer_config = {0};
    /* 获取时钟频率数值 */
    ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz((soc_module_clk_t)timg_config->clk_src, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &clk_src_hz));

    timer_config.alarm_en = TIMER_ALARM_EN;                   /* 警报使能 */
    timer_config.auto_reload = timg_config->auto_reload;      /* 自动重装载值 */
    timer_config.clk_src = timg_config->clk_src;              /* 设置时钟源 */
    timer_config.counter_dir = TIMER_COUNT_UP;                /* 向上计数 */
    timer_config.counter_en = TIMER_PAUSE;                    /* 停止定时器 */
    timer_config.divider = clk_src_hz / 1000000;              /* 预分频(1us) */

    /* 选择那个定时器组，组内那个定时器，并配置定时器 */
    ESP_ERROR_CHECK(timer_init(timg_config->timer_group, timg_config->timer_idx, &timer_config));
    /* 设置当前计数值 */
    ESP_ERROR_CHECK(timer_set_counter_value(timg_config->timer_group, timg_config->timer_idx, 0));
    /* 设置警报值 */
    ESP_ERROR_CHECK(timer_set_alarm_value(timg_config->timer_group, timg_config->timer_idx, timg_config->alarm_value));
    /* 使能定时器中断 */
    ESP_ERROR_CHECK(timer_enable_intr(timg_config->timer_group, timg_config->timer_idx));
    /* 添加定时器回调函数 */
    ESP_ERROR_CHECK(timer_isr_callback_add(timg_config->timer_group, timg_config->timer_idx, timer_group_isr_callback, timg_config, 0));
    /* 开启定时器 */
    ESP_ERROR_CHECK(timer_start(timg_config->timer_group, timg_config->timer_idx));
}
