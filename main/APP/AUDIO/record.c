// recorder.c
#include "record.h"
#include "driver/i2s.h"
#include "es8388.h"
#include "audioplay.h"
#include "esp_log.h"
#include "ff.h"   // 文件系统 API（必须）


#define TAG "recorder"

static FIL f_rec;                          // 文件对象
static uint8_t audio_buf[BUF_SIZE];       // 音频数据缓冲区
static uint8_t recording = 0;             // 录音标志位
static uint32_t g_wav_size = 0;           // 已录制的字节总数

static QueueHandle_t rec_cmd_queue;       // 控制命令队列
static SemaphoreHandle_t rec_mutex;       // 录音互斥锁



static void write_wav_header_placeholder(FIL *file)
{
    uint8_t wav_header[44] = {0};

    // "RIFF" chunk descriptor
    memcpy(&wav_header[0], "RIFF", 4);
    *(uint32_t *)&wav_header[4] = 0;  // Placeholder for file size
    memcpy(&wav_header[8], "WAVE", 4);

    // "fmt " sub-chunk
    memcpy(&wav_header[12], "fmt ", 4);
    *(uint32_t *)&wav_header[16] = 16;                // Subchunk1Size for PCM
    *(uint16_t *)&wav_header[20] = 1;                 // AudioFormat: PCM = 1
    *(uint16_t *)&wav_header[22] = 1;                 // NumChannels: Mono = 1
    *(uint32_t *)&wav_header[24] = 16000;             // SampleRate
    *(uint32_t *)&wav_header[28] = 16000 * 2;         // ByteRate = SampleRate * NumChannels * BitsPerSample/8
    *(uint16_t *)&wav_header[32] = 2;                 // BlockAlign = NumChannels * BitsPerSample/8
    *(uint16_t *)&wav_header[34] = 16;                // BitsPerSample

    // "data" sub-chunk
    memcpy(&wav_header[36], "data", 4);
    *(uint32_t *)&wav_header[40] = 0;  // Placeholder for data size

    UINT bw;
    f_write(file, wav_header, sizeof(wav_header), &bw);
}


// 在停止录音后修复 WAV 文件头
static void fix_wav_header(FIL *file, uint32_t data_size)
{
    uint32_t file_size = data_size + 36;

    f_lseek(file, 4);  // ChunkSize
    UINT bw;
    f_write(file, &file_size, 4, &bw);

    f_lseek(file, 40);  // Subchunk2Size
    f_write(file, &data_size, 4, &bw);
}


// 初始化录音用的 I2S 和 ES8388
static void init_rec_mode(void)
{
    myi2s_init();  // 初始化 I2S 外设
    es8388_adda_cfg(0, 1);  // 启用 ADC
    es8388_input_cfg(0);   // 输入通道设置为 MIC
    es8388_mic_gain(8);    // 设置麦克风增益
    es8388_alc_ctrl(3, 4, 4);  // 自动增益控制
    es8388_output_cfg(0, 0);   // 禁止输出通道
    es8388_spkvol_set(0);      // 静音输出
    es8388_i2s_cfg(0, 3);      // I2S 配置为标准模式
    i2s_set_samplerate_bits_sample(16000, I2S_BITS_PER_SAMPLE_16BIT); // 设置采样率和位宽
    i2s_trx_start();           // 启动 I2S
}

// 开始录音
static void start_recording(void)
{
    FRESULT res;
    FF_DIR rec_dir;

    // 尝试打开录音目录，如果不存在则创建
    res = f_opendir(&rec_dir, "0:/RECORDER");
    if (res != FR_OK) {
        res = f_mkdir("0:/RECORDER");
        if (res != FR_OK) {
            ESP_LOGE(TAG, "创建录音目录失败: %d", res);
            return;
        }
    }

    // 初始化录音硬件（I2S + ES8388）
    init_rec_mode();

    // 打开文件，准备写入
    res = f_open(&f_rec, "0:/RECORDER/REC00001.wav", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "无法打开录音文件: %d", res);
        return;
    }

    // 可选：写入WAV文件头（占位，停止录音后补充大小）
    // UINT bw;
    // f_write(&f_rec, &wavhead, sizeof(wavhead), &bw);
    // 写入 WAV 占位头
    write_wav_header_placeholder(&f_rec);

    // 设置标志位
    recording = 1;
    g_wav_size = 0;

    ESP_LOGI(TAG, "开始录音: REC00001.wav");
}


// 停止录音并关闭文件
static void stop_recording(void)
{
    if (!recording) {
        ESP_LOGW(TAG, "录音尚未开始，忽略停止操作");
        return;
    }

    recording = 0;

    // 判断文件是否已经打开
    if (f_rec.obj.fs != NULL) {
        fix_wav_header(&f_rec, g_wav_size);  // 修复 WAV 头
        f_close(&f_rec);
    }

    // 停止 I2S 硬件
    i2s_trx_stop();
    i2s_deinit();  // 卸载 I2S 驱动

    ESP_LOGI(TAG, "录音已停止");
}

// 播放录音文件
static void play_recording(void)
{
    FRESULT res;
    FIL test_file;

    // 播放前检查文件是否存在
    res = f_open(&test_file, "0:/RECORDER/REC00001.wav", FA_READ);
    if (res != FR_OK) {
        ESP_LOGW(TAG, "播放失败，录音文件不存在");
        return;
    }
    f_close(&test_file);

    // 调用播放接口
    ESP_LOGI(TAG, "开始播放 REC00001.wav");
    audio_play_song((uint8_t *)"0:/RECORDER/REC00001.wav");
    ESP_LOGI(TAG, "播放完成");
}



// 按键扫描任务，发送对应命令到队列
static void key_task(void *param)
{
    uint8_t key;
    recorder_cmd_t cmd;
    while (1) {
        key = xl9555_key_scan(0);  // 扫描按键
        switch (key) {
            case KEY0_PRES:
                ESP_LOGI(TAG, "KEY0_PRES pressed!");
                cmd = REC_CMD_START;
                xQueueSend(rec_cmd_queue, &cmd, portMAX_DELAY);
                break;
            case KEY1_PRES:
                ESP_LOGI(TAG, "KEY1_PRES pressed!");
                cmd = REC_CMD_STOP;
                xQueueSend(rec_cmd_queue, &cmd, portMAX_DELAY);
                break;
            case KEY2_PRES:
                ESP_LOGI(TAG, "KEY2_PRES pressed!");
                cmd = REC_CMD_PLAY;
                xQueueSend(rec_cmd_queue, &cmd, portMAX_DELAY);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 主任务：从命令队列中取出指令并执行
static void recorder_main_task(void *param)
{
    recorder_cmd_t cmd;
    while (1) {
        if (xQueueReceive(rec_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(rec_mutex, portMAX_DELAY);  // 互斥访问
            switch (cmd) {
                case REC_CMD_START:
                    start_recording();
                    ESP_LOGI(TAG, "start_recording!");
                    break;
                case REC_CMD_STOP:
                    stop_recording();
                    ESP_LOGI(TAG, "stop_recording!");
                    break;
                case REC_CMD_PLAY:
                    play_recording();
                    ESP_LOGI(TAG, "play_recording!");
                    break;
            }
            xSemaphoreGive(rec_mutex);
        }
    }
}

// 后台录音任务，将音频缓冲写入文件
static void recorder_data_task(void *param)
{
    while (1) {
        if (recording) {
            xSemaphoreTake(rec_mutex, portMAX_DELAY);
            size_t bytes_read = i2s_rx_read(audio_buf, BUF_SIZE);  // 从 I2S 读取数据
            if (bytes_read > 0) {
                UINT bw;
                f_write(&f_rec, audio_buf, bytes_read, &bw);  // 写入文件
                g_wav_size += bytes_read;
            }
            xSemaphoreGive(rec_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 初始化录音系统，创建任务、信号量等
void my_recorder_init(void)
{
    rec_cmd_queue = xQueueCreate(10, sizeof(recorder_cmd_t));
    rec_mutex = xSemaphoreCreateMutex();

    xTaskCreate(key_task, "key_task", 4096, NULL, 5, NULL);
    xTaskCreate(recorder_main_task, "rec_main", 4096*2, NULL, 6, NULL);
    xTaskCreate(recorder_data_task, "rec_data", 4096, NULL, 4, NULL);
}

