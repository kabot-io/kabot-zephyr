cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr)

project(app)
target_sources(app PRIVATE src/main.c)
