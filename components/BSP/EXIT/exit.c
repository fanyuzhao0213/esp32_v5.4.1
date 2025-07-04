/**
 ****************************************************************************************************
 * @file        exit.c
 */

#include "exit.h"
#include "led.h"


/**
 * @brief       外部中断服务函数
 * @param       arg：中断引脚号
 * @note        IRAM_ATTR: 这里的IRAM_ATTR属性用于将中断处理函数存储在内部RAM中，目的在于减少延迟
 * @retval      无
 */
static void IRAM_ATTR exit_gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;

    if (gpio_num == MY_KEY)
    {
        /* 消抖 */
        esp_rom_delay_us(20000);

        if (KEY_INT == 0)
        {
            LED0_TOGGLE();
        }
    }
}

/**
 * @brief       外部中断初始化程序
 * @param       无
 * @retval      无
 */
void exit_init(void)
{
    gpio_config_t gpio_init_struct;

    /* 配置BOOT引脚和外部中断 */
    gpio_init_struct.mode = GPIO_MODE_INPUT;                    /* 选择为输入模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;           /* 上拉使能 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;      /* 下拉失能 */
    gpio_init_struct.intr_type = GPIO_INTR_NEGEDGE;             /* 下降沿触发 */
    gpio_init_struct.pin_bit_mask = 1ull << MY_KEY;  /* 设置的引脚的位掩码 */
    ESP_ERROR_CHECK(gpio_config(&gpio_init_struct));            /* 配置使能 */

    /* 注册中断服务 */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    /* 设置BOOT的中断回调函数 */
    ESP_ERROR_CHECK(gpio_isr_handler_add(MY_KEY, exit_gpio_isr_handler, (void*) MY_KEY));
}
