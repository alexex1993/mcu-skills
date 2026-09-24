# Globbed so that --full and --minimal share one file: the minimal variant is
# the same tree with the extra .c files deleted.
file(GLOB app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "." "../include")
