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
            src/control/control_service.c
            src/zbus/channels/control_channel.c
            src/zbus/control/effort_subscriber.c
            src/zbus/channels/state_channel.c
            src/zbus/channels/state_egress_channel.c
            src/zbus/state/state_aggregator.c
            src/zbus/control/effort_state_publisher.c
            src/zbus/state/state_periodic_publisher.c
            src/zbus/state/state_udp_sender.c
            src/zbus/channels/sensor_channel.c
            src/zbus/state/sensor_subscriber.c
            src/encoder_sensor_service.c
)

if(CONFIG_KABOT_ENABLE_SIMULATED_STATE_SENSORS)
    if(CONFIG_KABOT_ENABLE_SIMULATED_IMU_PUBLISHER)
        target_sources(app PRIVATE src/zbus/state/sim_imu_publisher.c)
    endif()

    if(CONFIG_KABOT_ENABLE_SIMULATED_MAG_PUBLISHER)
        target_sources(app PRIVATE src/zbus/state/sim_magnetometer_publisher.c)
    endif()

    if(CONFIG_KABOT_ENABLE_SIMULATED_DISTANCE_PUBLISHER)
        target_sources(app PRIVATE src/zbus/state/sim_distance_publisher.c)
    endif()
endif()

if(CONFIG_KABOT_ENABLE_DISTANCE_PUBLISHER)
    target_sources(app PRIVATE src/zbus/state/distance_publisher.c)
endif()

if(CONFIG_KABOT_ENABLE_MAGNETOMETER_PUBLISHER)
    target_sources(app PRIVATE src/zbus/state/magnetometer_publisher.c)
endif()

if(CONFIG_KABOT_ENABLE_IMU_PUBLISHER)
    target_sources(app PRIVATE src/zbus/state/imu_publisher.c)
endif()

if(NOT BOARD MATCHES "^native_sim(/.*)?$")
    target_sources(
        app
        PRIVATE src/zbus/state/encoder_publisher.c
    )
endif()
