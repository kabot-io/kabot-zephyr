zephyr_library()

zephyr_include_directories(include)
zephyr_library_include_directories(include)

zephyr_library_sources(
    drivers/motor/esc_driver.c
    drivers/motor/sim_motor_driver.c
)

zephyr_library_sources_ifdef(
    CONFIG_KABOT_MOTOR_SHELL
    subsys/motor/motor_shell.c
)
