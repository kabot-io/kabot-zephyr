cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED)

project(kabot)

zephyr_library_include_directories(include)

target_sources(app PRIVATE
    src/main.c
    src/motor_service.c
    src/zbus/effort_channel.c
    src/zbus/effort_subscriber.c
)
