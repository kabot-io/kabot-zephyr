cmake_minimum_required(VERSION 3.20.5)

# list(APPEND ZEPHYR_EXTRA_MODULES
#     ${CMAKE_CURRENT_SOURCE_DIR}/../modules/lib/micro_ros_zephyr/modules/libmicroros
# )
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app)
target_sources(app PRIVATE src/main.c)
