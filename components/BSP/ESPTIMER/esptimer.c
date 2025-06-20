/**
 ****************************************************************************************************
 * @file        esptim.c
 */

#include "esptimer.h"
#include "led.h"

static const char *TAG = "ESPTIMER";
static esp_timer_handle_t oneshot_timer;

/**
 * @brief       定时器回调函数
 * @param       arg: 不携带参数
 * @retval      无
 */
void esptimer_callback(void *arg)
{
    // ESP_LOGI(TAG, "1秒单次定时器callback!");
    // LED0_TOGGLE();
}

/**
 * @brief       初始化高分辨率定时器(ESP_TIMER)
 * @param       tps: 定时器周期,以微秒为单位(μs).
 *              若以一秒为定时器周期来执行一次定时器中断,那此处tps = 1s = 1000000μs
 * @retval      无
 */

void esptimer_init(uint64_t tps)
{
    esp_timer_handle_t esp_tim_handle;                      /* 定义定时器句柄  */

    /* 定义一个定时器结构体设置定时器配置参数 */
    esp_timer_create_args_t timer_arg = {
        .callback = &esptimer_callback,                     /* 计时时间到达时执行的回调函数 */
        .arg = NULL,                                        /* 传递给回调函数的参数 */
        .dispatch_method = ESP_TIMER_TASK,                  /* 进入回调方式,从定时器任务进入 */
        .name = "Timer",                                    /* 定时器名称 */
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_arg, &esp_tim_handle));     /* 创建定时器 */
    ESP_ERROR_CHECK(esp_timer_start_periodic(esp_tim_handle, tps));     /* 启动周期性定时器,tps设置定时器周期(us单位) */

    ESP_LOGI(TAG, "1S定时器已启动");
}



// 定时器回调函数
static void timer_callback(void* arg)
{
    ESP_LOGI(TAG, "5秒定时器触发! 计数器完成");

    // 可选：在这里执行定时器到期后的操作
    // 例如：触发事件、改变GPIO状态等

    // 单次定时器不需要手动删除，执行完自动停止
}

void once_timer_init(uint64_t tps)
{
    // 定时器配置
    const esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,  // 回调函数
        .arg = NULL,                  // 传递给回调的参数
        .name = "5s_oneshot_timer"    // 定时器名称
    };

    // 创建定时器
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &oneshot_timer));

    // 启动单次定时器 (5秒 = 5,000,000 微秒)
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, tps));

    ESP_LOGI(TAG, "5秒单次定时器已启动");
}