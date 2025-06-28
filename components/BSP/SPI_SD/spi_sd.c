/**
 ****************************************************************************************************
 * @file        spi_sdcard.c

 ****************************************************************************************************
 */

#include "spi_sd.h"
#include <dirent.h>  // 用于 opendir, readdir
#include <string.h>  // 用于 strstr, strcmp

sdmmc_card_t *card;                                                 /* SD / MMC卡结构 */
const char mount_point[] = MOUNT_POINT;                             /* 挂载点/根目录 */
esp_err_t ret = ESP_OK;
esp_err_t mount_ret = ESP_FAIL;


// 列出目录下文件的辅助函数
void list_dir_files(const char *path, const char *dir_name)
{
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, dir_name);

    DIR *subdir = opendir(full_path);
    if (subdir) {
        struct dirent *entry;
        ESP_LOGI("SD_SPI", "目录 %s 中的文件列表:", full_path);
        while ((entry = readdir(subdir)) != NULL) {
            // 过滤掉 '.' 和 '..'
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                ESP_LOGI("SD_SPI", "文件: %s", entry->d_name);
            }
        }
        closedir(subdir);
    } else {
        ESP_LOGW("SD_SPI", "无法打开目录: %s", full_path);
    }
}

/**
 * @brief       SD卡初始化
 * @param       无
 * @retval      esp_err_t
 */
esp_err_t sd_spi_init(void)
{
    esp_err_t ret = ESP_OK;

    if (MY_SD_Handle != NULL)
    {
        if (mount_ret == ESP_OK)
        {
            ESP_LOGI("SD_SPI", "已挂载，准备卸载 SD 卡文件系统");
            esp_vfs_fat_sdcard_unmount(mount_point, card);
            mount_ret = ESP_FAIL;
            ESP_LOGI("SD_SPI", "SD 卡已卸载");
        }
    }
    else if (MY_SD_Handle == NULL)
    {
        my_spi_init();
        ESP_LOGI("SD_SPI", "SPI 驱动初始化完成");
    }

    /* 文件系统挂载配置 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 4 * 1024 * sizeof(uint8_t)
    };

    /* SD 卡参数配置 */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    /* SD 卡引脚配置 */
    sdspi_device_config_t slot_config = {0};
    slot_config.host_id  = host.slot;
    slot_config.gpio_cs  = SD_NUM_CS;
    slot_config.gpio_cd  = GPIO_NUM_NC;
    slot_config.gpio_wp  = GPIO_NUM_NC;
    slot_config.gpio_int = GPIO_NUM_NC;

    mount_ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (mount_ret == ESP_OK)
    {
        // 🔍 列出 SD 卡目录文件
        DIR *dir = opendir(mount_point);
        if (dir != NULL)
        {
            struct dirent *entry;
            ESP_LOGI("SD_SPI", "SD 卡根目录文件列表:");

            while ((entry = readdir(dir)) != NULL)
            {
                ESP_LOGI("SD_FILE", "文件: %s", entry->d_name);
                // 判断是否为 MP3 或 MUSIC 目录，递归列出里面文件
                if (strcasecmp(entry->d_name, "MP3") == 0 || strcasecmp(entry->d_name, "MUSIC") == 0) {
                    ESP_LOGI("SD_SPI", "MP3 OR MUSIC!");
                    list_dir_files(mount_point, entry->d_name);
                }
            }
        }
        ESP_LOGI("SD_SPI", "SD 卡文件系统挂载成功");
    }
    else
    {
        ESP_LOGE("SD_SPI", "SD 卡文件系统挂载失败，错误码: 0x%x (%s)", mount_ret, esp_err_to_name(mount_ret));
    }

    ret |= mount_ret;

    vTaskDelay(pdMS_TO_TICKS(10));

    if (ret == ESP_OK)
    {
        ESP_LOGI("SD_SPI", "SD 卡初始化成功");
    }
    else
    {
        ESP_LOGE("SD_SPI", "SD 卡初始化失败");
    }

    return ret;
}


/**
 * @brief       获取SD卡相关信息
 * @param       out_total_bytes：总大小
 * @param       out_free_bytes：剩余大小
 * @retval      无
 */
void sd_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes)
{
    FATFS *fs;
    size_t free_clusters;
    int res = f_getfree("0:", (DWORD *)&free_clusters, &fs);
    assert(res == FR_OK);

    size_t total_sectors = (fs->n_fatent - 2) * fs->csize;
    size_t free_sectors = free_clusters * fs->csize;

    size_t sd_total_kb = total_sectors * fs->ssize / 1024;
    size_t sd_free_kb  = free_sectors * fs->ssize / 1024;
    size_t sd_total_mb = sd_total_kb / 1024;
    size_t sd_free_mb  = sd_free_kb / 1024;

    if (out_total_bytes != NULL)
    {
        *out_total_bytes = sd_total_kb * 1024;  // 转换回字节
    }

    if (out_free_bytes != NULL)
    {
        *out_free_bytes = sd_free_kb * 1024;    // 转换回字节
    }

    // 打印信息
    ESP_LOGI("SD_USAGE", "SD 卡总容量: %d KB (%d MB)", (int)sd_total_kb, (int)sd_total_mb);
    ESP_LOGI("SD_USAGE", "SD 卡剩余容量: %d KB (%d MB)", (int)sd_free_kb, (int)sd_free_mb);
}