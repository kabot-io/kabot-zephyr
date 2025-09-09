# https://micro.ros.org/docs/tutorials/core/first_application_rtos/zephyr/

cd ~/workspace

git clone -b $ROS_DISTRO https://github.com/micro-ROS/micro_ros_setup.git src/micro_ros_setup

sudo apt update && rosdep update
rosdep install --from-paths src --ignore-src -y
sudo apt-get install python3-pip

colcon build
source install/local_setup.bash

ros2 run micro_ros_setup create_firmware_ws.sh zephyr olimex-stm32-e407 # there is no micro_ros_setup for ESP32-S3

# git clone -b $ROS_DISTRO github.com/micro-ROS/micro_ros_zephyr_module modules/lib/micro_ros
# west build -b esp32s3_devkitm modules/lib/micro_ros -p
# west flash
