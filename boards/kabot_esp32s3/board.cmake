# 1. Serial Flasher (esp-flash) - Crucial for "west flash --runner esp-flash"
board_runner_args(esp-flash "--no-stub")
board_runner_args(esp-flash "--chip" "esp32s3")

# 2. OpenOCD (JTAG)
board_runner_args(openocd "--cmd-pre-init" "source [find board/esp32s3-builtin.cfg]")
board_runner_args(openocd "--cmd-pre-init" "init")

include(${ZEPHYR_BASE}/boards/common/esp32.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
