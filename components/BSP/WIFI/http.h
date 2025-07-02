#ifndef HTTP_H
#define HTTP_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>
#include "esp_netif_ip_addr.h"  // 提供 esp_ip4addr_ntoa()
#include "esp_http_client.h"


void http_get_task(void *pvParameters);
void decode_mp3_task(void *pvParameters);


static int ringbuf_write(const uint8_t *data, int len);
static int ringbuf_read(uint8_t *buf, int len);


#endif