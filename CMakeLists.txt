cmake_minimum_required(VERSION 3.16)

project(pet_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WIN32 => GUI subsystem (no console window) on both MSVC and MinGW-w64.
add_executable(pet_demo WIN32 main.cpp)

# Resolves to gdiplus.lib on MSVC and -lgdiplus on MinGW-w64.
target_link_libraries(pet_demo PRIVATE gdiplus)
