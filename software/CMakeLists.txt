cmake_minimum_required(VERSION 3.12)

# Pull in Pi Pico SDK (must be before project)
include(pico-sdk/pico_sdk_init.cmake)

project(ft-vac-ctrl C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.c
    decoder.c
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

# create map/bin/hex/u2f file etc.
pico_add_extra_outputs(${PROJECT_NAME})

# pull in common dependencies
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
)