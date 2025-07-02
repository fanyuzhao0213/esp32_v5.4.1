/**
 * @file wifi.c
 * @brief ESP32 WiFi dual mode (STA + AP) with WiFi configuration and synchronization support
 */

#include "wifi.h"


// AP 模式的默认参数
#define WIFI_AP_SSID      "ESP32-AP"
#define WIFI_AP_PASS      "12345678"
#define WIFI_AP_CHANNEL   1
#define WIFI_AP_MAX_CONN  4

#define WIFI_TAG "WIFI"


// 创建 WiFi 事件组句柄（main.c 中可用此等待 WiFi 连接完成）
EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;  // WiFi连接成功事件位
// 保存 STA 模式配置的静态结构体
static wifi_config_t sta_config;

/**
 * @brief WiFi 事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();  // STA启动后立即尝试连接
    }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(WIFI_TAG, "STA 断开连接，原因: %d，尝试重连...", disconn->reason);
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[16];  // IPv4 地址最大长度为 15 + 1
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(WIFI_TAG, "STA 获取到 IP 地址: %s", ip_str);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);  // 设置事件位，通知连接完成
    }
}

/**
 * @brief 初始化 WiFi（STA + AP 模式）
 */
void wifi_init_sta_ap(void)
{
    // 初始化网络接口和事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建默认的 STA 和 AP 网络接口
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // 初始化 WiFi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册 WiFi 和 IP 事件回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // 初始化事件组（用于任务同步）
    wifi_event_group = xEventGroupCreate();

    // 配置 AP 模式参数
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    // 默认 STA 配置（如未主动设置，将使用此配置）
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;

    // 设置为双模式（AP + STA）
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "WiFi STA + AP 模式已启动,AP SSID: %s, STA SSID: %s", WIFI_AP_SSID, sta_config.sta.ssid);
}

/**
 * @brief 设置 STA 模式下的 WiFi 配置
 * @param ssid     STA连接的WiFi名称
 * @param password STA连接的密码
 */
void wifi_config_sta(const char *ssid, const char *password)
{
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));
    ESP_LOGI(WIFI_TAG, "STA配置更新完成,SSID: %s", ssid);
}


void my_wifi_init(void)
{
    wifi_config_sta(STA_SSID, STA_PASSWORD);
    // 2. 初始化 STA+AP 模式
    wifi_init_sta_ap();
    // 等待STA连接成功
    ESP_LOGI("MAIN", "等待WiFi连接...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI("MAIN", "WiFi连接成功, 开始网络业务...");

}


