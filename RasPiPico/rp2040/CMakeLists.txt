cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
#include(pico_extras_import_optional.cmake)

project(fdark_pico_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(PICO_NO_FLASH 1)
#pico_set_linker_script(${BinName} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_copy_to_ram.ld)
#set(PICO_COPY_TO_RAM 1)


if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.1")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

#set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

#list(APPEND PICO_CONFIG_HEADER_FILES ${PROJECT_SOURCE_DIR}/sys_clock.h)

# Initialize the SDK
pico_sdk_init()

add_definitions(-DNDEBUG)

add_compile_options(
		-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

# Hardware-specific examples in subdirectories:
add_subdirectory(playlib)

add_subdirectory(mgspico)
#add_subdirectory(mgspico_2)
#add_subdirectory(mgspico_3)


