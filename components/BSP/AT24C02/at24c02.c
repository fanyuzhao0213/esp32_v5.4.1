/**
 ****************************************************************************************************
 * @file        at24c02.c
 ****************************************************************************************************
 */

#include "at24c02.h"

const uint8_t g_text_buf[] = {"FYZ_ESP32-S3_EEPROM_24C02"}; /* 要写入到 24c02 的字符串数组 */
#define TEXT_SIZE sizeof(g_text_buf) /* TEXT 字符串长度 */


i2c_master_dev_handle_t eeprom_handle = NULL;
const char* at24c02_tag = "at24c02";

static const char *TAG = "IIC_EEPROM";

void my_iic_eeprom_test(void)
{
    esp_err_t ret = ESP_OK;
    uint8_t read_buf[TEXT_SIZE] = {0};
    bool need_write = false;

    // 写入数据
    for (uint16_t i = 0; i < TEXT_SIZE; i++)
    {
        at24c02_write_one_byte(i, g_text_buf[i]);

        // 验证写入
        uint8_t verify = at24c02_read_one_byte(i);
        if (verify != g_text_buf[i])
        {
            ESP_LOGE(TAG, "EEPROM write verify failed at addr %u! Wrote: 0x%02X, Read: 0x%02X", i, g_text_buf[i], verify);
            ret = ESP_FAIL;
            break;
        }
    }

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "EEPROM data written successfully and verified.");
    }
    else
    {
        ESP_LOGE(TAG, "EEPROM write test failed!");
    }

    ESP_LOGI(TAG, "Reading EEPROM data at addr 0...");

    // 读取 EEPROM 数据
    for (uint16_t i = 0; i < TEXT_SIZE; i++)
    {
        read_buf[i] = at24c02_read_one_byte(i);
    }
    ESP_LOGI(TAG, "EEPROM read addr 0 data :%s",read_buf);
}

/**
 * @brief       初始化AT24C02
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t at24c02_init(void)
{
    /* 未调用myiic_init初始化IIC */
    if (bus_handle == NULL)
    {
        ESP_ERROR_CHECK(myiic_init());
    }

    i2c_device_config_t eeprom_i2c_dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,  /* 从机地址长度 */
        .scl_speed_hz    = IIC_SPEED_CLK,       /* 传输速率 */
        .device_address  = AT_ADDR,             /* 从机7位的地址 */
    };
    /* I2C总线上添加AT24C02设备 */
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &eeprom_i2c_dev_conf, &eeprom_handle));

    return ESP_OK;
}

/**
 * @brief       在AT24C02指定地址读出一个数据
 * @param       addr: 开始读数的地址
 * @retval      读到的数据
 */
uint8_t at24c02_read_one_byte(uint8_t addr)
{
    uint8_t data = 0;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(eeprom_handle, &addr, 1, &data, 1, -1));

    return data;
}

/**
 * @brief       在AT24C02指定地址写入一个数据
 * @param       addr: 写入数据的目的地址
 * @param       data: 要写入的数据
 * @retval      无
 */
void at24c02_write_one_byte(uint8_t addr, uint8_t data)
{
    uint8_t send_buf[2] = {0};

    send_buf[0] = addr % 256;
    send_buf[1] = data;

    ESP_ERROR_CHECK(i2c_master_transmit(eeprom_handle, send_buf, 2, -1));

    /* 由于AT24C02写入过慢，需延迟10ms左右时间 */
    esp_rom_delay_us(10000);
}

/**
 * @brief       检查AT24C02是否正常
 * @note        检测原理: 在器件的末地址写如0X55, 然后再读取, 如果读取值为0X55
 *              则表示检测正常. 否则,则表示检测失败.
 * @param       无
 * @retval      检测结果
 *              0: 检测成功
 *              1: 检测失败
 */
uint8_t at24c02_check(void)
{
    uint8_t temp;
    uint16_t addr = AT24C02;

    temp = at24c02_read_one_byte(addr);     /* 避免每次开机都写AT24CXX */

    if (temp == 0X55)                       /* 读取数据正常 */
    {
        return 0;
    }
    else                                    /* 排除第一次初始化的情况 */
    {
        at24c02_write_one_byte(addr, 0X55); /* 先写入数据 */
        temp = at24c02_read_one_byte(255);  /* 再读取数据 */

        if (temp == 0X55)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief       在AT24C02里面的指定地址开始读出指定个数的数据
 * @param       addr    : 开始读出的地址 对24c02为0~255
 * @param       pbuf    : 数据数组首地址
 * @param       datalen : 要读出数据的个数
 * @retval      无
 */
void at24c02_read(uint8_t addr, uint8_t *pbuf, uint8_t datalen)
{
    while (datalen--)
    {
        *pbuf++ = at24c02_read_one_byte(addr++);
    }
}

/**
 * @brief       在AT24C02里面的指定地址开始写入指定个数的数据
 * @param       addr    : 开始写入的地址 对24c02为0~255
 * @param       pbuf    : 数据数组首地址
 * @param       datalen : 要写入数据的个数
 * @retval      无
 */
void at24c02_write(uint8_t addr, uint8_t *pbuf, uint8_t datalen)
{
    while (datalen--)
    {
        at24c02_write_one_byte(addr, *pbuf);
        addr++;
        pbuf++;
    }
}
