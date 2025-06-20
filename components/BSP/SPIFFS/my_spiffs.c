/**
 ****************************************************************************************************
 * @file        my_spiffs.c
 ****************************************************************************************************
 */

#include "my_spiffs.h"


static const char *spiffs_tag = "spiffs";

/**
 * @brief       spiffs初始化
 * @param       partition_label:分区表的分区名称
 * @param       mount_point:文件系统关联的文件路径前缀
 * @param       max_files:可以同时打开的最大文件数
 * @retval      ESP_OK:成功; ESP_FAIL:失败
 */
esp_err_t spiffs_init(char *partition_label, char *mount_point, size_t max_files)
{
    size_t total = 0;   /* SPIFFS总容量 */
    size_t used = 0;    /* SPIFFS已使用的容量 */

    esp_vfs_spiffs_conf_t spiffs_conf = {           /* 配置spiffs文件系统的参数 */
        .base_path              = mount_point,      /* 磁盘路径,比如"0:","1:" */
        .partition_label        = partition_label,  /* 分区表的分区名称 */
        .max_files              = max_files,        /* 最大可同时打开的文件数 */
        .format_if_mount_failed = true,             /* 挂载失败则格式化文件系统 */
    };

    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);  /* 初始化和挂载SPIFFS分区 */
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(spiffs_tag, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(spiffs_tag, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(spiffs_tag, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }

        return ESP_FAIL;
    }

    ret = esp_spiffs_info(spiffs_conf.partition_label, &total, &used);  /* 获取SPIFFS的总容量和已使用的容量 */
    if (ret != ESP_OK)
    {
        ESP_LOGI(spiffs_tag, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(spiffs_tag, "Partition size: total: %d Bytes, used: %d Bytes", total, used);
    }

    return ret;
}

/**
 * @brief       注销spiffs
 * @param       partition_label:分区表的分区名称
 * @retval      ESP_OK:注销成功; 其他:失败
 */
esp_err_t spiffs_deinit(char *partition_label)
{
    return esp_vfs_spiffs_unregister(partition_label);
}



#define WRITE_DATA              "ALIENTEK ESP32-S3"     /* 写入数据 */

/**
 * @brief       测试spiffs
 * @param       无
 * @retval      无
 */
void spiffs_test(void)
{
    ESP_LOGI("spiffs_test", "Opening file");

    FILE *file_obj = fopen("/spiffs/hello.txt", "w");   /* 建立一个名为/spiffs/hello.txt的只写文件 */
    if (file_obj == NULL)
    {
        ESP_LOGE("spiffs_test", "Failed to open file for writing");
    }

    fprintf(file_obj, WRITE_DATA);      /* 写入字符 */
    fclose(file_obj);                   /* 关闭文件 */

    /* 重命名之前检查目标文件是否存在 */
    struct stat st;
    if (stat("/spiffs/foo.txt", &st) == 0)  /* 获取文件信息，获取成功返回0 */
    {
        /*  从文件系统中删除一个名称。
            如果名称是文件的最后一个连接，并且没有其它进程将文件打开，
            名称对应的文件会实际被删除。 */
        unlink("/spiffs/foo.txt");
    }

    /* 重命名创建的文件 */
    if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0)
    {
        ESP_LOGE("spiffs_test", "Rename failed");
    }

    /* 打开重命名的文件并读取 */
    file_obj = fopen("/spiffs/foo.txt", "r");
    if (file_obj == NULL)
    {
        ESP_LOGE("spiffs_test", "Failed to open file for reading");
    }

    char line[64];
    fgets(line, sizeof(line), file_obj);    /* 从指定的流中读取数据 */
    fclose(file_obj);

    char *pos = strchr(line, '\n'); /* 指针pos指向第一个找到‘\n’ */
    if (pos)
    {
        *pos = '\0';                /* 将‘\n’替换为‘\0’ */
    }

    ESP_LOGI("spiffs_test", "Read from file: '%s'", line);


    /* 列出 SPIFFS 挂载目录中的文件 */
    ESP_LOGI("spiffs_test", "Listing files in /spiffs:");
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE("spiffs_test", "Failed to open directory /spiffs");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI("spiffs_test", "Found file: %s", entry->d_name);
    }
    closedir(dir);
}

