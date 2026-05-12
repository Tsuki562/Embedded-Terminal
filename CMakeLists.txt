# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# The configuration is to prevent compilation errors on the Windows system.
set(CMAKE_OBJECT_PATH_MAX 512)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS "")
set(PROJECT_COMMON_COMPONENT_DIR_CANDIDATES
    "${CMAKE_CURRENT_LIST_DIR}/../common_components"
    "${CMAKE_CURRENT_LIST_DIR}/../esp-dev-kits-master/esp-dev-kits-master/examples/esp32-p4-function-ev-board/examples/common_components"
)

foreach(COMMON_COMPONENT_DIR IN LISTS PROJECT_COMMON_COMPONENT_DIR_CANDIDATES)
    if(EXISTS "${COMMON_COMPONENT_DIR}")
        list(APPEND EXTRA_COMPONENT_DIRS "${COMMON_COMPONENT_DIR}")
        break()
    endif()
endforeach()

if(NOT EXTRA_COMPONENT_DIRS)
    message(FATAL_ERROR "Could not find common_components. Expected one of: ${PROJECT_COMMON_COMPONENT_DIR_CANDIDATES}")
endif()

set(CXX_COMPILE_OPTIONS
    -Wno-deprecated-enum-enum-conversion
    -Wno-write-strings
)

add_compile_options(
    -Wno-format
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-unused-but-set-variable
    -Wno-cast-function-type
    -Wno-ignored-qualifiers
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-enum-enum-conversion")

project(esp_brookesia_demo VERSION 1.0.1)
