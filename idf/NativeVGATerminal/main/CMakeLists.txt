# the component is named after the directory this repository has been cloned into
get_filename_component(FABGL_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(FABGL_COMPONENT "${FABGL_COMPONENT}" NAME)

idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES ${FABGL_COMPONENT})
