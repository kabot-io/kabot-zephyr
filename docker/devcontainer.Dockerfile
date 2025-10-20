ARG UBUNTU_DISTRO="noble"
FROM mcr.microsoft.com/devcontainers/base:${UBUNTU_DISTRO}

ARG ROS_DISTRO
ENV ROS_DISTRO=${ROS_DISTRO}
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupmod -aG ${USER_GID} vscode \
    && usermod -u ${USER_UID} -g ${USER_GID} -aG dialout vscode

WORKDIR /workspaces

RUN apt-get update \
    && apt-get install curl -y \
    && export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}') \
    && curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb" \
    && dpkg -i /tmp/ros2-apt-source.deb \
    && apt-get update \
    && apt-get install -y \
        ros-dev-tools \
        ros-${ROS_DISTRO}-ros-base \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update \
    && apt-get install -y \
        ccache \
        clang-format \
        clangd \
        cmake-format \
        device-tree-compiler \
        dfu-util \
        file \
        g++-multilib \
        gcc-multilib \
        gperf \
        ninja-build \
        python3-venv \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN echo 'source /opt/ros/${ROS_DISTRO}/setup.zsh' | sudo tee -a /etc/zsh/zshrc
RUN echo 'source $(find /workspaces/*/.venv/bin/activate)' | sudo tee -a /etc/zsh/zshrc

USER vscode
SHELL ["/usr/bin/zsh", "-c"]
