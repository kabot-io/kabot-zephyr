zephyr_library()

zephyr_library_sources_ifdef(
    CONFIG_KABOT_LED_STRIP_SHELL
    subsys/led_strip/led_strip_shell.c
)
