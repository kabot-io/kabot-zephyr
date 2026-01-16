#!/bin/bash

if [ ! -d ".venv" ]; then
    python3 -m venv .venv
fi

source .venv/bin/activate

pip install west -q

if [ ! -d ".west" ]; then
    west init --local .
fi

west update
west packages pip --install
west zephyr-export 