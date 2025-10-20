# kabot-zephyr

This repository is configured to use a `.devcontainer` for development. To ensure a consistent and reliable environment, you must open this repository using Visual Studio Code with the Dev Containers extension.

## Getting Started

1. **Open the Repository in VS Code**:
    Make sure you have the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) installed. Open this repository in VS Code, and it will automatically set up the development container.

2. **Build Firmware**:
    Once the development container is ready, you can build the firmware for all supported architectures by running the following script:

    ```zsh
    ./scripts/build.zsh
    ```

    This script is guaranteed to work within the development container.

## Requirements

- Visual Studio Code
- Dev Containers extension

By using the provided `.devcontainer`, you ensure that all dependencies and tools are correctly configured for this project.
