#include "http.h"
#include <ctype.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"

#define HTTP_TAG "HTTP"

#define MAX_MP3_FILES 10
#define MAX_FILENAME_LEN 128
#define HTML_BUF_SIZE 4096
#define RINGBUF_SIZE (8 * 1024)

typedef enum {
    HTTP_TASK_TYPE_HTML,
    HTTP_TASK_TYPE_MP3
} http_task_type_t;

static uint8_t ringbuf[RINGBUF_SIZE];
static int write_pos = 0;
static int read_pos = 0;

static SemaphoreHandle_t data_sem;
static SemaphoreHandle_t ringbuf_mutex;

static char mp3_filenames[MAX_MP3_FILES][MAX_FILENAME_LEN];
static int mp3_file_count = 0;

static http_task_type_t current_http_task_type = HTTP_TASK_TYPE_HTML;

static char html_buf[HTML_BUF_SIZE];
static int html_buf_offset = 0;


// 添加这行函数声明，放在所有函数定义前
esp_err_t _http_event_handler(esp_http_client_event_t *evt);


// URL 解码
void url_decode(char *dst, const char *src, int max_len)
{
    char a, b;
    int i = 0;
    while (*src && i < max_len - 1) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            a = tolower(a);
            b = tolower(b);
            a = (a >= 'a') ? (a - 'a' + 10) : (a - '0');
            b = (b >= 'a') ? (b - 'a' + 10) : (b - '0');
            *dst++ = 16 * a + b;
            src += 3;
        } else {
            *dst++ = *src++;
        }
        i++;
    }
    *dst = '\0';
}

// HTML 中解析 MP3 文件列表
void parse_mp3_file_list(const char *html, int len)
{
    const char *p = html;
    mp3_file_count = 0;

    while ((p = strstr(p, "href=\"")) != NULL && mp3_file_count < MAX_MP3_FILES) {
        p += 6;
        const char *end = strchr(p, '"');
        if (!end) break;

        int raw_len = end - p;
        if (raw_len >= MAX_FILENAME_LEN) raw_len = MAX_FILENAME_LEN - 1;

        char encoded[MAX_FILENAME_LEN];
        char decoded[MAX_FILENAME_LEN];

        strncpy(encoded, p, raw_len);
        encoded[raw_len] = '\0';

        url_decode(decoded, encoded, MAX_FILENAME_LEN);

        if (strstr(decoded, ".mp3")) {
            strncpy(mp3_filenames[mp3_file_count], encoded, MAX_FILENAME_LEN);
            mp3_filenames[mp3_file_count][MAX_FILENAME_LEN - 1] = '\0';
            mp3_file_count++;
        }

        p = end + 1;
    }

    ESP_LOGI("MP3", "共发现 %d 个 MP3 文件", mp3_file_count);
    for (int i = 0; i < mp3_file_count; i++) {
        ESP_LOGI("MP3", "[%d] %s", i + 1, mp3_filenames[i]);
    }
}

// MP3 文件下载任务
void http_download_mp3_task(void *pvParameters)
{
    for (int i = 0; i < mp3_file_count; i++) {
        current_http_task_type = HTTP_TASK_TYPE_MP3;

        char full_url[256];
        snprintf(full_url, sizeof(full_url), "http://192.168.0.25:8000/%.127s", mp3_filenames[i]);


        ESP_LOGI("MP3", "准备下载: %s", full_url);

        esp_http_client_config_t config = {
            .url = full_url,
            .event_handler = _http_event_handler,
            .timeout_ms = 5000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            int content_length = esp_http_client_get_content_length(client);
            ESP_LOGI("MP3", "下载完成: Status = %d, length = %d", status, content_length);
        } else {
            ESP_LOGE("MP3", "下载失败: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    vTaskDelete(NULL);
}

// HTTP 事件处理
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(HTTP_TAG, "已连接");
            break;

        case HTTP_EVENT_ON_HEADER:
            if (strcmp(evt->header_key, "Content-Length") == 0) {
                int file_size = atoi(evt->header_value);
                ESP_LOGI(HTTP_TAG, "文件大小: %d 字节", file_size);
            }
            break;

        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                const uint8_t *data = (const uint8_t *)evt->data;

                if (current_http_task_type == HTTP_TASK_TYPE_HTML) {
                    if (html_buf_offset + evt->data_len < HTML_BUF_SIZE) {
                        memcpy(html_buf + html_buf_offset, data, evt->data_len);
                        html_buf_offset += evt->data_len;
                    }
                } else if (current_http_task_type == HTTP_TASK_TYPE_MP3) {
                    int written = ringbuf_write(data, evt->data_len);
                    if (written > 0) {
                        xSemaphoreGive(data_sem);
                    }
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            if (current_http_task_type == HTTP_TASK_TYPE_HTML) {
                html_buf[html_buf_offset] = '\0';
                parse_mp3_file_list(html_buf, html_buf_offset);
                html_buf_offset = 0;
                xTaskCreate(http_download_mp3_task, "http_mp3_task", 8192, NULL, 5, NULL);
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

// 启动 HTML 获取任务
void http_get_task(void *pvParameters)
{
    data_sem = xSemaphoreCreateBinary();
    ringbuf_mutex = xSemaphoreCreateMutex();

    current_http_task_type = HTTP_TASK_TYPE_HTML;
    html_buf_offset = 0;

    esp_http_client_config_t config = {
        .url = "http://192.168.0.25:8000/",
        .event_handler = _http_event_handler,
        .user_data = html_buf,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "HTML 获取失败: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

// 解码任务（仅作为示例）
void decode_mp3_task(void *pvParameters)
{
    uint8_t buffer[256];
    int len;

    while (1) {
        if (xSemaphoreTake(data_sem, portMAX_DELAY) == pdTRUE) {
            while ((len = ringbuf_read(buffer, sizeof(buffer))) > 0) {
                // 在此处理 buffer 数据，如解码播放等
            }
        }
    }
}

// 写入环形缓冲区
static int ringbuf_write(const uint8_t *data, int len)
{
    xSemaphoreTake(ringbuf_mutex, portMAX_DELAY);

    int free_space = (write_pos >= read_pos)
                     ? RINGBUF_SIZE - (write_pos - read_pos) - 1
                     : (read_pos - write_pos) - 1;

    if (len > free_space) len = free_space;

    for (int i = 0; i < len; i++) {
        ringbuf[write_pos] = data[i];
        write_pos = (write_pos + 1) % RINGBUF_SIZE;
    }

    xSemaphoreGive(ringbuf_mutex);
    return len;
}

// 读取环形缓冲区
static int ringbuf_read(uint8_t *buf, int len)
{
    xSemaphoreTake(ringbuf_mutex, portMAX_DELAY);

    int data_len = (write_pos >= read_pos)
                   ? (write_pos - read_pos)
                   : (RINGBUF_SIZE - (read_pos - write_pos));

    if (len > data_len) len = data_len;

    for (int i = 0; i < len; i++) {
        buf[i] = ringbuf[read_pos];
        read_pos = (read_pos + 1) % RINGBUF_SIZE;
    }

    xSemaphoreGive(ringbuf_mutex);
    return len;
}
