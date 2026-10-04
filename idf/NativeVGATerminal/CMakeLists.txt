cmake_minimum_required(VERSION 3.16)

# FabGL is used as component directly from this repository
get_filename_component(FABGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${FABGL_DIR}")
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/../sdkconfig.fabgl")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(NativeVGATerminal)
