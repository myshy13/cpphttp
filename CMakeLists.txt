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

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(cpphttp PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(cpphttp PRIVATE /opt/homebrew/include)

target_include_directories(cpphttp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
