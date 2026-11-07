cmake_minimum_required(VERSION 3.16)
project(Groundstation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network SerialPort Test Widgets)

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

set(BACKEND_INCLUDE_DIRS
    ${SRC_DIR}/backend
    ${SRC_DIR}/backend/include
    ${SRC_DIR}/backend/src
)
set(BACKEND_LIBS Qt6::Core Qt6::Network Qt6::SerialPort)

target_include_directories(groundstation PRIVATE
    ${BACKEND_INCLUDE_DIRS}
    ${SRC_DIR}/ui/include
)

target_link_libraries(groundstation PRIVATE ${BACKEND_LIBS} Qt6::Widgets)

# Sprites (assets/sprites/*.png), fonts (assets/fonts/*.ttf|otf) and offline maps
# (assets/maps/*.json) are compiled into the binary, so the app runs from anywhere
# (e.g. autostart on the Pi). They're read in code as :/sprites/<name>.png,
# :/fonts/<file> and :/maps/<file>.
file(GLOB ASSET_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.otf
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/*.json
)
if(ASSET_FILES)
    qt_add_resources(groundstation "assets"
        PREFIX "/"
        BASE ${CMAKE_CURRENT_SOURCE_DIR}/assets
        FILES ${ASSET_FILES}
    )
endif()

# Backend unit tests: run with ctest --test-dir build
set(BACKEND_SOURCES ${SOURCES})
list(FILTER BACKEND_SOURCES INCLUDE REGEX ".*/src/backend/.*")

enable_testing()
add_executable(test_backend tests/test_backend.cpp ${BACKEND_SOURCES})
target_include_directories(test_backend PRIVATE ${BACKEND_INCLUDE_DIRS})
target_link_libraries(test_backend PRIVATE ${BACKEND_LIBS} Qt6::Test)
add_test(NAME test_backend COMMAND test_backend)
