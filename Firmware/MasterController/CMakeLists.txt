cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME MasterController)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)


# TINY USB SOURCE
set(TINYUSB_DIR ${CMAKE_SOURCE_DIR}/Core/Src/TinyUSB)
include(${TINYUSB_DIR}/CMakeLists.txt)

# The stack plus every class. Classes that tusb_config.h doesn't enable compile to nothing.
tinyusb_sources_get(TINYUSB_SRC)

# TinyUSB's list leaves out the hardware driver. These two are the driver for the G4's USB block.
list(APPEND TINYUSB_SRC
    ${TINYUSB_DIR}/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    ${TINYUSB_DIR}/portable/st/stm32_fsdev/fsdev_common.c
)


# ADD THESE FOLDERS!
file(GLOB APP_SRC CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Core/Src/FreeRTOS_apps/*.c
    ${CMAKE_SOURCE_DIR}/Core/Src/hardware_drivers/*.c   # for drivers like the bucket's, when you add them
)
# macOS leaves "._name.c" metadata files on drives that aren't Mac-formatted; they aren't C
list(FILTER APP_SRC EXCLUDE REGEX "/\\._[^/]*$")


# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${APP_SRC}
    ${TINYUSB_SRC}
)

# Add include paths (Core/Inc, which holds tusb_config.h, already comes from cmake/stm32cubemx)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${TINYUSB_DIR}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
