#!/usr/bin/zsh

python3 -m venv .venv
source .venv/bin/activate
pip install -U pip
pip install west
west update
west packages pip --install
west zephyr-export
