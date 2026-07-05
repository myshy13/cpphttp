cmake_minimum_required(VERSION 3.12)
project(cpphttp LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    src/main.cpp
    src/server.cpp
)

add_executable(cpphttp ${SOURCE_FILES})

target_include_directories(cpphttp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
