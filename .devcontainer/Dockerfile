ARG IMAGE_NAME
ARG IMAGE_TAG

FROM ghcr.io/kabot-io/kabot-zephyr/${IMAGE_NAME}:${IMAGE_TAG}

USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    tio \
    && rm -rf /var/lib/apt/lists/*

USER vscode
