# Minimalna wersja CMake wymagana do zbudowania projektu
# Dlaczego 3.15? Bo to stabilna wersja z dobrym wsparciem dla C
cmake_minimum_required(VERSION 3.15)

# Definicja projektu: nazwa i język
# LeafTS - nazwa projektu (możesz użyć w zmiennych ${PROJECT_NAME})
# C - używamy tylko języka C (nie C++)
project(LeafTS C)

# Standard języka C
# C11 - nowoczesny standard z atomics, _Static_assert, itp.
set(CMAKE_C_STANDARD 11)
# REQUIRED - wymusza C11, nie pozwala na starsze wersje
set(CMAKE_C_STANDARD_REQUIRED ON)
# OFF - wyłącza rozszerzenia GNU (używamy czystego ISO C)
set(CMAKE_C_EXTENSIONS OFF)

# Tworzenie pliku wykonywalnego
# add_executable(NAZWA_PROGRAMU lista_plików_źródłowych)
# leafts - nazwa końcowego pliku (leafts.exe na Windows)
add_executable(leafts
    main.c
    src/hal_vflash.c
    src/leafts.c
)

# Ścieżki do plików nagłówkowych (.h)
target_include_directories(leafts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Test executable - Unity framework + CTest
enable_testing()

add_executable(leafts_test
    test/test.c
    src/hal_vflash.c
    src/leafts.c
    unity/unity.c
)
target_include_directories(leafts_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
)
add_test(NAME LeafTS_Tests COMMAND leafts_test)

# UART SERVER TARGET - TCP SOCKET VIRTUAL UART + uart_handler
# ON REAL MCU: REPLACE hal_vuart.c WITH hal_stm32_uart.c OR hal_esp32_uart.c
add_executable(leafts_uart
    main_uart.c
    src/hal_vflash.c
    src/hal_vuart.c
    src/leafts.c
    src/uart_handler.c
)
target_include_directories(leafts_uart PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# LINK WINSOCK2 ON WINDOWS - REQUIRED FOR TCP SOCKETS
if(WIN32)
    target_link_libraries(leafts_uart PRIVATE ws2_32)
endif()
