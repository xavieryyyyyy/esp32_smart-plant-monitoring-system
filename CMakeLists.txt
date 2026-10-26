# Start the ESP-IDF build and give the firmware project its name.
# Require a CMake version that supports this ESP-IDF project configuration.
cmake_minimum_required(VERSION 3.16.0)
# Load ESP-IDF's build helpers from the IDF_PATH environment variable.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Name the firmware project; ESP-IDF discovers its application/components during this setup.
project(plant-monitor)
