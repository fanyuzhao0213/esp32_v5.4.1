#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>
#include "esp_netif_ip_addr.h"  // 提供 esp_ip4addr_ntoa()
#include "esp_http_client.h"

#define STA_SSID            "TPP"              // 替换为你实际要连接的热点
#define STA_PASSWORD        "td88888888"       // 替换为你实际要连接的热点


// WiFi事件组和事件位（用于同步）
extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;


/**
 * @brief 初始化 WiFi（STA + AP 共存）
 */
void wifi_init_sta_ap(void);

/**
 * @brief 设置 STA 模式的 WiFi 名称和密码
 *
 * @param ssid     WiFi 名称
 * @param password WiFi 密码
 */
void wifi_config_sta(const char *ssid, const char *password);

void my_wifi_init(void);


#endif // WIFI_H

