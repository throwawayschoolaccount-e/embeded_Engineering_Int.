cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(ota_gateway CXX C ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(ota_gateway
    main.cpp
    uart_link.cpp
    http_server.cpp
)

target_include_directories(ota_gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(ota_gateway
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_uart
)

pico_enable_stdio_usb(ota_gateway 1)
pico_enable_stdio_uart(ota_gateway 0)

pico_add_extra_outputs(ota_gateway)
