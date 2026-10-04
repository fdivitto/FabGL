cmake_minimum_required(VERSION 3.16)

# Builds one of the FabGL Arduino examples with ESP-IDF, using the Arduino core as IDF component.
# The SKETCH environment variable selects the example (see main/CMakeLists.txt).

# FabGL is used as component directly from this repository
get_filename_component(FABGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${FABGL_DIR}")

# configuration: common FabGL settings, Arduino settings and, when present, the ones of the sketch.
# sdkconfig is kept inside the build directory, so every build directory has its own.
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/../sdkconfig.fabgl;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(SKETCH "$ENV{SKETCH}")
if(NOT SKETCH)
  set(SKETCH "VGA/SimpleTerminalOut")
endif()
if(NOT IS_ABSOLUTE "${SKETCH}")
  set(SKETCH "${FABGL_DIR}/examples/${SKETCH}")
endif()
if(EXISTS "${SKETCH}/sdkconfig.defaults")
  list(APPEND SDKCONFIG_DEFAULTS "${SKETCH}/sdkconfig.defaults")
endif()
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# the Arduino core does not always build warning-free against the latest release of its managed components
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=missing-field-initializers;-Wno-error=cpp" APPEND)

project(ArduinoSketch)
