/**
 ****************************************************************************************************
 * @file        xl9555.h
 ****************************************************************************************************
 */

#ifndef __XL9555_H
#define __XL9555_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "myiic.h"
#include "string.h"


/* 引脚与相关参数定义 */
#define XL9555_INT_IO               GPIO_NUM_40                     /* XL9555_INT引脚 */

/* XL9555寄存器宏 */
#define XL9555_INPUT_PORT0_REG      0                               /* 输入寄存器0地址 */
#define XL9555_INPUT_PORT1_REG      1                               /* 输入寄存器1地址 */
#define XL9555_OUTPUT_PORT0_REG     2                               /* 输出寄存器0地址 */
#define XL9555_OUTPUT_PORT1_REG     3                               /* 输出寄存器1地址 */
#define XL9555_INVERSION_PORT0_REG  4                               /* 极性反转寄存器0地址 */
#define XL9555_INVERSION_PORT1_REG  5                               /* 极性反转寄存器1地址 */
#define XL9555_CONFIG_PORT0_REG     6                               /* 方向配置寄存器0地址 */
#define XL9555_CONFIG_PORT1_REG     7                               /* 方向配置寄存器1地址 */

#define XL9555_ADDR                 0X20                            /* XL9555器件7位地址-->请看手册（9.1. Device Address） */


#define AP_INT_IO                   0x0001  // 二进制: 0000 0000 0000 0001 -> bit0
#define QMA_INT_IO                  0x0002  // 二进制: 0000 0000 0000 0010 -> bit1
#define SPK_EN_IO                   0x0004  // 二进制: 0000 0000 0000 0100 -> bit2
#define BEEP_IO                     0x0008  // 二进制: 0000 0000 0000 1000 -> bit3
#define OV_PWDN_IO                  0x0010  // 二进制: 0000 0000 0001 0000 -> bit4
#define OV_RESET_IO                 0x0020  // 二进制: 0000 0000 0010 0000 -> bit5
#define GBC_LED_IO                  0x0040  // 二进制: 0000 0000 0100 0000 -> bit6
#define GBC_KEY_IO                  0x0080  // 二进制: 0000 0000 1000 0000 -> bit7
#define LCD_BL_IO                   0x0100  // 二进制: 0000 0001 0000 0000 -> bit8
#define CT_RST_IO                   0x0200  // 二进制: 0000 0010 0000 0000 -> bit9
#define SLCD_RST_IO                 0x0400  // 二进制: 0000 0100 0000 0000 -> bit10
#define SLCD_PWR_IO                 0x0800  // 二进制: 0000 1000 0000 0000 -> bit11
#define KEY3_IO                     0x1000  // 二进制: 0001 0000 0000 0000 -> bit12
#define KEY2_IO                     0x2000  // 二进制: 0010 0000 0000 0000 -> bit13
#define KEY1_IO                     0x4000  // 二进制: 0100 0000 0000 0000 -> bit14
#define KEY0_IO                     0x8000  // 二进制: 1000 0000 0000 0000 -> bit15


#define KEY0                        xl9555_pin_read(KEY0_IO)        /* 读取KEY0引脚 */
#define KEY1                        xl9555_pin_read(KEY1_IO)        /* 读取KEY1引脚 */
#define KEY2                        xl9555_pin_read(KEY2_IO)        /* 读取KEY2引脚 */
#define KEY3                        xl9555_pin_read(KEY3_IO)        /* 读取KEY3引脚 */

#define KEY0_PRES                   1                               /* KEY0按下 */
#define KEY1_PRES                   2                               /* KEY1按下 */
#define KEY2_PRES                   3                               /* KEY1按下 */
#define KEY3_PRES                   4                               /* KEY1按下 */

/* 函数声明 */
esp_err_t xl9555_init(void);                                            /* 初始化XL9555 */
int xl9555_pin_read(uint16_t pin);                                      /* 获取某个IO状态 */
uint16_t xl9555_pin_write(uint16_t pin, int val);                       /* 控制某个IO的电平 */
esp_err_t xl9555_read_byte(uint8_t* data, size_t len);                  /* 读取XL9555的IO值 */
esp_err_t xl9555_write_byte(uint8_t reg, uint8_t *data, size_t len);    /* 向XL9555寄存器写入数据 */
uint8_t xl9555_key_scan(uint8_t mode);                                  /* 扫描扩展按键 */
void xl9555_int_init(void);                                             /* 初始化XL9555的中断引脚 */

#endif
