ARG UBUNTU_DISTRO="noble"
FROM mcr.microsoft.com/devcontainers/base:${UBUNTU_DISTRO}

ARG USER_UID=1000
ARG USER_GID=1000

RUN groupmod -g ${USER_GID} vscode \
    && usermod -u ${USER_UID} -g ${USER_GID} vscode

WORKDIR /workspaces

RUN apt-get update \
    && apt-get install -y \
    ccache \
    clang-format \
    clangd \
    cmake \
    cmake-format \
    device-tree-compiler \
    dfu-util \
    fd-find \
    file \
    g++-multilib \
    gcc-multilib \
    gperf \
    ninja-build \
    protobuf-compiler \
    python3-venv \
    shellcheck \
    tio \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

USER vscode
SHELL ["/usr/bin/zsh", "-c"]
