cmake_minimum_required(VERSION 3.16)
project(Groundstation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${SRC_DIR}/app/*.cpp
    ${SRC_DIR}/backend/*.cpp
    ${SRC_DIR}/backend/*.h
    ${SRC_DIR}/ui/*.cpp
    ${SRC_DIR}/ui/*.h
    ${SRC_DIR}/ui/*.ui
)

# Tests have their own main()
list(FILTER SOURCES EXCLUDE REGEX ".*/tests/.*")

# Skip generated files if someone builds inside the source tree
list(FILTER SOURCES EXCLUDE REGEX ".*/(CMakeFiles|[^/]*_autogen)/.*")

# Pi-only SPI source (adjust filename to match yours)
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm")
    list(FILTER SOURCES EXCLUDE REGEX ".*/Spi[^/]*$")
endif()

add_executable(groundstation ${SOURCES})

# .ui files live in src/ui, not next to the .cpp files in src/ui/src
set_target_properties(groundstation PROPERTIES
    AUTOUIC_SEARCH_PATHS ${SRC_DIR}/ui
)

target_include_directories(groundstation PRIVATE
    ${SRC_DIR}/backend
    ${SRC_DIR}/ui/include
)

target_link_libraries(groundstation PRIVATE Qt6::Core Qt6::Widgets)
