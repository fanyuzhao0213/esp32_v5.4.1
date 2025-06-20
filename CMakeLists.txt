# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
# 功能：设置额外的组件搜索路径
set(EXTRA_COMPONENT_DIRS components/Middlewares)
# 这个选项的作用是让编译器（GCC 或 Clang）在输出诊断信息（如警告和错误）时总是使用颜色，使得在终端中更容易识别。
add_compile_options(-fdiagnostics-color=always)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/spiffs)

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.


project(fyz_esp32_project)
