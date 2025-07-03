/**
 ****************************************************************************************************
 * @file        record.h
 ****************************************************************************************************
 */

#ifndef __RECORD_H
#define __RECORD_H

#include "es8388.h"
#include "myi2s.h"
#include "xl9555.h"
#include "ff.h"
#include "wavplay.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define BUF_SIZE 4096  // 音频缓冲区大小

// 控制命令类型枚举
typedef enum {
    REC_CMD_START,  // 开始录音
    REC_CMD_STOP,   // 停止录音
    REC_CMD_PLAY    // 播放录音
} recorder_cmd_t;

// 初始化录音模块
void my_recorder_init(void);

#endif // __RECORDER_H__




