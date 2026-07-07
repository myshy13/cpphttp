cmake_minimum_required(VERSION 3.12)

project(cpphttp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    src/main.cpp
    src/server.cpp
    include/tls.cpp
)

# Create the executable
add_executable(cpphttp ${SOURCE_FILES})

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(cpphttp PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

target_compile_definitions(cpphttp PRIVATE
    ENABLE_TLS
)

target_include_directories(cpphttp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /opt/homebrew/include
)

# Convenience target to build and run tests
# (Only works if test_exe exists somewhere else)
add_custom_target(test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_exe
    DEPENDS test_exe
    COMMENT "Building and running unit tests"
)