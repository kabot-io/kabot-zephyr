cmake_minimum_required(VERSION 3.20.5)

# list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../modules/lib/micro_ros_zephyr/modules/libmicroros )
find_package(Zephyr REQUIRED)
if(CONFIG_SOC_ESP32S3)
    test_sysbuild()
endif()

project(app)
target_sources(app PRIVATE src/main.c)
