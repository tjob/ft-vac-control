cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(vctrl C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.c
    decoder.c
)

# create map/bin/hex/u2f file etc.
pico_add_extra_outputs(${PROJECT_NAME})

# pull in common dependencies
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
)