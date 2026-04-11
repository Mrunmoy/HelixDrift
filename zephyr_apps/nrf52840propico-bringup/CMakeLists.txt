cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(helix_nrf52840propico_bringup)

target_sources(app PRIVATE
  src/main.cpp
)
