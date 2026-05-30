zephyr_library()

zephyr_include_directories(include)
zephyr_library_include_directories(include)
zephyr_syscall_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_KABOT_ESC_MOTOR drivers/motor/esc_driver.c)
zephyr_library_sources_ifdef(CONFIG_KABOT_H_BRIDGE_MOTOR drivers/motor/h_bridge_driver.c)
zephyr_library_sources_ifdef(CONFIG_KABOT_SIM_MOTOR drivers/motor/sim_motor_driver.c)

zephyr_library_sources_ifdef(
    CONFIG_KABOT_MOTOR_SHELL
    subsys/motor/motor_shell.c
)
