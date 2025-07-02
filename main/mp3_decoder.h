#ifndef MP3_DECODER_H
#define MP3_DECODER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_player.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "es8388.h"
#include "esp_check.h"
#include "audioplay.h"
#include "xl9555.h"
#include "freertos/queue.h"

esp_err_t play_mp3_file(FILE *fp);
#endif // MP3_DECODER_H
