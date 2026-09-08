cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(epi)

target_sources(app PRIVATE
  src/main.c
  src/ble_epi.c
  ../lib/epi_led.c
  ../lib/epi_battery.c
  ../lib/epi_button.c
  ../lib/epi_detector.c
)
target_include_directories(app PRIVATE ../lib)
