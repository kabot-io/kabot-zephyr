#!/usr/bin/zsh

CONTAINER="ros_kilted_microros_tutorial"
ROS_IMG="ros:kilted"
SHARED_DIR="$(git rev-parse --show-toplevel)/.microros-tutorial-workspace"

if [ "$(sudo docker ps -a -q -f name=^/${CONTAINER}$)" ]; then
    sudo docker start -ai ${CONTAINER}
else
    sudo docker run -it --net=host -v ${SHARED_DIR}:/root/workspace --privileged --name ${CONTAINER} ${ROS_IMG}
fi
