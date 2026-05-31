cmake_minimum_required(VERSION 3.20.5)


list(
    APPEND
    ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/../modules/motor_driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../modules/led_strip
    )

find_package(Zephyr REQUIRED)

project(kabot)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

zephyr_nanopb_sources(app protos/effort_msg.proto)

zephyr_library_include_directories(include)

target_sources(
    app
    PRIVATE src/main.c
            src/motor/motor_service.c
            src/zbus/effort_channel.c
            src/zbus/effort_subscriber.c
            src/zbus/encoder_publisher.c
            src/zbus/sensor_channel.c
            src/zbus/sensor_subscriber.c
            src/encoder_sensor_service.c
)
