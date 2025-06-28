/**
 ****************************************************************************************************
 * @file        wavplay.c
 */

#include "wavplay.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************************************/
/*FreeRTOS配置*/

/* MUSIC 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define MUSIC_PRIO      4                   /* 任务优先级 */
#define MUSIC_STK_SIZE  5*1024              /* 任务堆栈大小 */
TaskHandle_t            MUSICTask_Handler;  /* 任务句柄 */
void music(void *pvParameters);             /* 任务函数 */

static portMUX_TYPE my_spinlock = portMUX_INITIALIZER_UNLOCKED;

/******************************************************************************************************/

__wavctrl wavctrl;                          /* WAV音频文件解码参数结构体 */
UINT bytes_write = 0;                       /* 写一次I2S大小 */
volatile long long int i2s_table_size = 0;  /* 积累每次发送音频数据总大小 */
esp_err_t i2s_play_end = ESP_FAIL;          /* 播放结束标志位 */
esp_err_t i2s_play_next_prev = ESP_FAIL;    /* 下一首或者上一首标志位 */
FSIZE_t file_read_pos = 0;                  /* 记录当前WAV读取位置 */

/**
 * @brief       WAV 文件解析初始化
 * @param       fname : 文件路径+文件名
 * @param       wavx  : WAV 文件信息存放结构体指针
 * @retval      0, 打开文件成功且解析正确
 *              1, 打开文件失败
 *              2, 非WAV文件
 *              3, DATA区域未找到
 *              4, 内存分配失败
 */
uint8_t wav_decode_init(uint8_t *fname, __wavctrl *wavx)
{
    FIL *ftemp = NULL;
    uint8_t *buf = NULL;
    uint32_t br = 0;
    uint8_t res = 0;

    ChunkRIFF *riff = NULL;
    ChunkFMT *fmt = NULL;
    ChunkFACT *fact = NULL;
    ChunkDATA *data = NULL;

    // 打印开始解析信息
    printf("[WAV Parser] Starting to parse file: %s\n", fname);

    // 分配内存
    ftemp = (FIL*)malloc(sizeof(FIL));
    buf = malloc(512);

    if (!ftemp || !buf) {
        printf("[WAV Parser] Error: Memory allocation failed\n");
        res = 4;
        goto cleanup;
    }

    // 打开文件
    res = f_open(ftemp, (TCHAR*)fname, FA_READ);
    if (res != FR_OK) {
        printf("[WAV Parser] Error: Failed to open file (Error code: %d)\n", res);
        res = 1;
        goto cleanup;
    }

    // 读取文件头部
    res = f_read(ftemp, buf, 512, (UINT *)&br);
    if (res != FR_OK || br < 512) {
        printf("[WAV Parser] Error: Failed to read file header (Read: %lu bytes)\n", br);
        res = 1;
        goto cleanup;
    }

    riff = (ChunkRIFF *)buf;

    // 检查是否是WAV文件 (RIFF格式和WAVE标识)
    if (riff->Format != 0x45564157) {  // "WAVE"的十六进制表示
        printf("[WAV Parser] Error: Not a valid WAV file (Format: 0x%ld)\n", riff->Format);
        res = 2;
        goto cleanup;
    }

    printf("[WAV Parser] Valid WAV file detected\n");

    // 解析FMT块
    fmt = (ChunkFMT *)(buf + 12);  // RIFF头之后是FMT块
    printf("[WAV Parser] FMT Chunk Size: %lu\n", fmt->ChunkSize);

    // 检查FACT块是否存在 (某些WAV文件可能有)
    fact = (ChunkFACT *)(buf + 12 + 8 + fmt->ChunkSize);
    if (fact->ChunkID == 0x74636166 ||  // "fact"
        fact->ChunkID == 0x5453494C) {  // "LIST"
        printf("[WAV Parser] FACT/LIST Chunk found (Size: %lu)\n", fact->ChunkSize);
        wavx->datastart = 12 + 8 + fmt->ChunkSize + 8 + fact->ChunkSize;
    } else {
        printf("[WAV Parser] No FACT/LIST Chunk found\n");
        wavx->datastart = 12 + 8 + fmt->ChunkSize;
    }

    // 查找DATA块
    data = (ChunkDATA *)(buf + wavx->datastart);
    if (data->ChunkID != 0x61746164) {  // "data"
        printf("[WAV Parser] Error: DATA Chunk not found at offset %lu\n", wavx->datastart);
        res = 3;
        goto cleanup;
    }

    // 填充WAV文件信息结构体
    wavx->audioformat = fmt->AudioFormat;
    wavx->nchannels = fmt->NumOfChannels;
    wavx->samplerate = fmt->SampleRate;
    wavx->bitrate = fmt->ByteRate * 8;
    wavx->blockalign = fmt->BlockAlign;
    wavx->bps = fmt->BitsPerSample;
    wavx->datasize = data->ChunkSize;
    wavx->datastart += 8;  // 跳过DATA块的头部

    // 打印详细的WAV文件信息
    printf("\n[WAV Parser] File Information:\n");
    printf("  Audio Format:    %s (%d)\n",
           wavx->audioformat == 1 ? "PCM" : "Non-PCM",
           wavx->audioformat);
    printf("  Channels:        %d\n", wavx->nchannels);
    printf("  Sample Rate:     %lu Hz\n", wavx->samplerate);
    printf("  Bit Rate:        %lu bps\n", wavx->bitrate);
    printf("  Block Align:     %d bytes\n", wavx->blockalign);
    printf("  Bits Per Sample: %d\n", wavx->bps);
    printf("  Data Size:       %lu bytes\n", wavx->datasize);
    printf("  Data Start:      at offset %lu\n", wavx->datastart);
    printf("\n[WAV Parser] WAV file parsed successfully\n");

    res = 0;

cleanup:
    // 资源清理
    if (ftemp) {
        f_close(ftemp);
        free(ftemp);
    }
    if (buf) {
        free(buf);
    }

    return res;
}


/**
 * @brief       获取当前播放时间
 * @param       fx    : 文件指针
 * @param       wavx  : wavx播放控制器
 * @retval      无
 */
void wav_get_curtime(FIL *fx, __wavctrl *wavx)
{
    long long fpos = 0;

    wavx->totsec = wavx->datasize / (wavx->bitrate / 8);    /* 歌曲总长度(单位:秒) */
    fpos = fx->fptr-wavx->datastart;                        /* 得到当前文件播放到的地方 */
    wavx->cursec = fpos * wavx->totsec / wavx->datasize;    /* 当前播放到第多少秒了? */
}

/**
 * @brief       music任务
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void music(void *pvParameters)
{
    pvParameters = pvParameters;

    /* ES8388初始化配置，有效降低启动时发出沙沙声 */
    es8388_adda_cfg(1,0);                           /* 打开DAC，关闭ADC */
    es8388_input_cfg(0);                            /* 录音关闭 */
    es8388_output_cfg(1,1);                         /* 喇叭通道和耳机通道打开 */
    es8388_hpvol_set(10);                           /* 设置耳机 */
    es8388_spkvol_set(0);                          /* 设置喇叭 */
    xl9555_pin_write(SPK_EN_IO,0);                  /* 打开喇叭 */
    vTaskDelay(pdMS_TO_TICKS(20));
    i2s_tx_write(g_audiodev.tbuf, WAV_TX_BUFSIZE);  /* 先发送一段无声音的数据 */

    while(1)
    {
        if ((g_audiodev.status & 0x0F) == 0x03)     /* 打开了音频 */
        {
            for(uint16_t readTimes = 0; readTimes < (wavctrl.datasize / WAV_TX_BUFSIZE); readTimes++)
            {
                if ((g_audiodev.status & 0x0F) == 0x00)             /* 暂停播放 */
                {
                    file_read_pos = f_tell(g_audiodev.file);        /* 记录暂停位置 */

                    while(1)
                    {
                        if ((g_audiodev.status & 0x0F) == 0x03)     /* 重新打开了 */
                        {
                            break;
                        }

                        vTaskDelay(pdMS_TO_TICKS(5));               /* 死等 */
                    }

                    f_lseek(g_audiodev.file, file_read_pos);        /* 跳过到之前停止的位置 */
                }

                /* 判断是否播放完成 */
                if (i2s_table_size >= wavctrl.datasize || i2s_play_next_prev == ESP_OK)
                {
                    audio_stop();                   /* 先停止播放 */
                    i2s_deinit();                   /* 卸载I2S */
                    i2s_table_size = 0;             /* 总大小清零 */
                    i2s_play_end = ESP_OK;          /* 已播放完成标志位 */
                    vTaskDelete(NULL);              /* 删除当前任务 */
                    vTaskDelay(pdMS_TO_TICKS(5));   /* 适当延时（为了删除这个任务） */
                    break;                          /* 防止延时5ms未能删除音频任务 */
                }

                f_read(g_audiodev.file,g_audiodev.tbuf, WAV_TX_BUFSIZE, (UINT*)&bytes_write);
                i2s_table_size = i2s_table_size + i2s_tx_write(g_audiodev.tbuf, WAV_TX_BUFSIZE);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    vTaskDelete(NULL);
}

/**
 * @brief       播放某个wav文件
 * @param       fname : 文件路径+文件名
 * @retval      KEY0_PRES : 下一首
 *              KEY1_PRES : 上一首
 *              KEY2_PRES : 停止/启动
 *              其他,非WAV文件
 */
uint8_t wav_play_song(uint8_t *fname)
{
    uint8_t key = 0;
    uint8_t res = 0;

    i2s_play_end = ESP_FAIL;
    i2s_play_next_prev = ESP_FAIL;
    g_audiodev.file = (FIL*)heap_caps_malloc(sizeof(FIL),MALLOC_CAP_DMA);
    g_audiodev.tbuf = heap_caps_malloc(WAV_TX_BUFSIZE, MALLOC_CAP_DMA);       /* 音频数据 */

    myi2s_init();                                   /* I2S初始化 */
    vTaskDelay(pdMS_TO_TICKS(50));                  /* 适当延时 */

    if (g_audiodev.file || g_audiodev.tbuf)
    {
        memset(g_audiodev.file,0,sizeof(FIL));      /* 文件指针清零 */
        memset(g_audiodev.tbuf,0,WAV_TX_BUFSIZE);   /* buf清零 */
        memset(&wavctrl,0,sizeof(__wavctrl));       /* 对WAV结构体相关参数清零 */
        res = wav_decode_init(fname, &wavctrl);     /* 对wav音频文件解码 */
        /*如果解析失败，可以在这里添加退出或错误处理逻辑*/
        if(res != 0) {
            // 例如：返回错误、显示错误界面、记录日志等
            audio_stop();                   /* 先停止播放 */
            i2s_deinit();                   /* 卸载I2S */
            return KEY0_PRES;  // 或者其他适当的错误处理
        }

        if (res == 0)                               /* 解码成功 */
        {
            if (wavctrl.bps == 16)                  /* 根据解码文件重新配置采样率和位宽 */
            {
                i2s_set_samplerate_bits_sample(wavctrl.samplerate,I2S_BITS_PER_SAMPLE_16BIT);
            }
            else if (wavctrl.bps == 24)
            {
                i2s_set_samplerate_bits_sample(wavctrl.samplerate,I2S_BITS_PER_SAMPLE_24BIT);
            }

            res = f_open(g_audiodev.file, (TCHAR*)fname, FA_READ);      /* 打开WAV音频文件 */

            if (res == FR_OK)
            {
                audio_start();  /* 开启I2S */
                /* 打开成功后，才创建音频任务 */
                if (MUSICTask_Handler == NULL && res == FR_OK)
                {
                    taskENTER_CRITICAL(&my_spinlock);
                    xTaskCreate(music, "music", 4096, &MUSICTask_Handler, 5, NULL);
                    taskEXIT_CRITICAL(&my_spinlock);
                }

                while (res == FR_OK)
                {
                    while (1)
                    {
                        /* 播放结束，下一首 */
                        if (i2s_play_end == ESP_OK)
                        {
                            res = KEY0_PRES;
                            break;
                        }

                        key = xl9555_key_scan(0);

                        switch (key)
                        {
                            /* 下一首/上一首 */
                            case KEY0_PRES:
                            case KEY1_PRES:
                                i2s_play_next_prev = ESP_OK;
                                break;
                            /* 暂停/开启 */
                            case KEY2_PRES:
                                if ((g_audiodev.status & 0x0F) == 0x03)
                                {
                                    audio_stop();
                                }
                                else if ((g_audiodev.status & 0x0F) == 0x00)
                                {
                                    audio_start();
                                }
                                break;
                        }

                        if ((g_audiodev.status & 0x0F) == 0x03)                 /* 暂停不刷新时间 */
                        {
                            wav_get_curtime(g_audiodev.file, &wavctrl);         /* 得到总时间和当前播放的时间 */
                            audio_msg_show(wavctrl.totsec, wavctrl.cursec, wavctrl.bitrate);
                        }

                        vTaskDelay(pdMS_TO_TICKS(10));
                    }

                    if (key == KEY1_PRES || key == KEY0_PRES)                   /* 退出切换歌曲 */
                    {
                        res = key;
                        break;
                    }
                }
            }
            else
            {
                res = 0xFF;
            }
        }
        else
        {
            res = 0xFF;
        }
    }
    else
    {
        res = 0xFF;
    }

    heap_caps_free(g_audiodev.file);
    heap_caps_free(g_audiodev.tbuf);
    g_audiodev.tbuf = NULL;
    g_audiodev.file = NULL;
    MUSICTask_Handler = NULL;
    return res;
}
