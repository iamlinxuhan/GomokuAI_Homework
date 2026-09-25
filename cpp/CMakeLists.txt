# 五子棋计算服务端
#
#   Linux / macOS:  cmake -S . -B build && cmake --build build
#   Windows MSVC:   cmake -S . -B build -G "Visual Studio 17 2022"
#                   cmake --build build --config Release
#   Windows MinGW:  cmake -S . -B build -G "MinGW Makefiles" && cmake --build build
#
# 产物固定输出到 build/bin/，python/config.py 就去那儿找可执行文件。

cmake_minimum_required(VERSION 3.16)
project(gomoku_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型" FORCE)
endif()

add_executable(gomoku_server
  src/main.cpp
  src/game.cpp
  src/ai.cpp
  src/tcp_server.cpp
)

# third_party/json.hpp 是 nlohmann/json 的单头文件版本
target_include_directories(gomoku_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

find_package(Threads REQUIRED)
target_link_libraries(gomoku_server PRIVATE Threads::Threads)

if(WIN32)
  target_link_libraries(gomoku_server PRIVATE ws2_32)
endif()

if(MSVC)
  # /utf-8：源码和界面上的中文都得按 UTF-8 处理
  target_compile_options(gomoku_server PRIVATE /utf-8 /W4)
else()
  target_compile_options(gomoku_server PRIVATE -Wall -Wextra)
endif()

set_target_properties(gomoku_server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(gomoku_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin")
endforeach()

add_custom_command(TARGET gomoku_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "构建完成: $<TARGET_FILE:gomoku_server>")
