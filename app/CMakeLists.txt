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

zephyr_nanopb_sources(app protos/state_control_msg.proto)

zephyr_library_include_directories(include)

target_sources(
    app
    PRIVATE src/main.c
            src/motor/motor_service.c
            src/zbus/control_channel.c
            src/zbus/control_subscriber.c
            src/zbus/state_channel.c
            src/zbus/state_egress_channel.c
            src/zbus/state_aggregator.c
            src/zbus/effort_state_publisher.c
            src/zbus/state_periodic_publisher.c
            src/zbus/state_udp_sender.c
            src/zbus/sensor_channel.c
            src/zbus/sensor_subscriber.c
            src/encoder_sensor_service.c
)

if(CONFIG_KABOT_ENABLE_SIMULATED_STATE_SENSORS)
    target_sources(
        app
        PRIVATE src/zbus/sim_imu_publisher.c
                src/zbus/sim_magnetometer_publisher.c
                src/zbus/sim_distance_publisher.c
    )
endif()

if(NOT BOARD MATCHES "^native_sim(/.*)?$")
    target_sources(
        app
        PRIVATE src/zbus/encoder_publisher.c
    )
endif()
