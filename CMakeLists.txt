cmake_minimum_required(VERSION 3.20)
project(AquaTrack VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

include(cmake/Domain.cmake)
include(cmake/Data.cmake)
include(cmake/Inference.cmake)
include(cmake/Application.cmake)
include(cmake/Desktop.cmake)
include(cmake/Tests.cmake)
