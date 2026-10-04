# The SKETCH environment variable selects the example to build. It is the sketch directory, relative to
# the "examples" directory of this repository (or an absolute path). Example:
#
#   SKETCH=VGA/HardwareTest idf.py build flash monitor

get_filename_component(FABGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EXAMPLES_DIR "${FABGL_DIR}/examples")

# the FabGL component is named after the directory this repository has been cloned into
get_filename_component(FABGL_COMPONENT "${FABGL_DIR}" NAME)

set(SKETCH "$ENV{SKETCH}")
if(NOT SKETCH)
  set(SKETCH "VGA/SimpleTerminalOut")
endif()
if(IS_ABSOLUTE "${SKETCH}")
  set(SKETCH_DIR "${SKETCH}")
else()
  set(SKETCH_DIR "${EXAMPLES_DIR}/${SKETCH}")
endif()
get_filename_component(SKETCH_NAME "${SKETCH_DIR}" NAME)
set(SKETCH_INO "${SKETCH_DIR}/${SKETCH_NAME}.ino")
set(SKETCH_CPP "${CMAKE_CURRENT_BINARY_DIR}/${SKETCH_NAME}.ino.cpp")

# other sources of the sketch
file(GLOB_RECURSE SKETCH_SRCS "${SKETCH_DIR}/*.cpp" "${SKETCH_DIR}/*.c")

idf_component_register(SRCS "${SKETCH_CPP}" ${SKETCH_SRCS}
                       INCLUDE_DIRS "${SKETCH_DIR}"
                       REQUIRES ${FABGL_COMPONENT} arduino-esp32 esp_psram)

# examples are written for the Arduino IDE, which does not stop on warnings
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)

# the Arduino IDE converts .ino to .cpp adding function prototypes: do the same
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
  if(NOT EXISTS "${SKETCH_INO}")
    message(FATAL_ERROR "Sketch not found: ${SKETCH_INO}")
  endif()
  message(STATUS "Building sketch ${SKETCH_INO}")
  add_custom_command(OUTPUT "${SKETCH_CPP}"
                     COMMAND ${PYTHON} "${CMAKE_CURRENT_LIST_DIR}/../ino2cpp.py" "${SKETCH_INO}" "${SKETCH_CPP}"
                     DEPENDS "${SKETCH_INO}" "${CMAKE_CURRENT_LIST_DIR}/../ino2cpp.py"
                     VERBATIM)
endif()
