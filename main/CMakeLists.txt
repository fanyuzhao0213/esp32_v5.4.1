idf_component_register(
                        SRC_DIRS
                            "."
                            "APP"
                            "APP/AUDIO"
                        INCLUDE_DIRS
                            "."
                            "APP"
                            "APP/AUDIO")

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
# 添加组件依赖
# set(COMPONENT_REQUIRES esp_audio_codec)