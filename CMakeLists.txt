cmake_minimum_required(VERSION 3.16)
project(Groundstation VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_subdirectory(src/ui)
add_subdirectory(src/app)