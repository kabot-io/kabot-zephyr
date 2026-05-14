cmake_minimum_required(VERSION 3.20.5)

list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/../modules/motor_driver
)

find_package(Zephyr REQUIRED)

project(kabot)

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

target_sources_ifdef(CONFIG_KABOT_MOTOR_SHELL app PRIVATE src/motor/motor_shell.c)
