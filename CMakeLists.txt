cmake_minimum_required(VERSION 4.0)
project(Webify)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6 Widgets
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Use pkg-config to locate libvips
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED vips-cpp)

# Include libvips headers and libraries
include_directories(${VIPS_INCLUDE_DIRS})
link_directories(${VIPS_LIBRARY_DIRS})
add_definitions(${VIPS_CFLAGS_OTHER})

# Executable
add_executable(${PROJECT_NAME} main.cpp
        SizeRow.h
        Converter.cpp
        Converter.h
        Utils.cpp
        Utils.h
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets ${VIPS_LIBRARIES})

find_package(Qt6 COMPONENTS Core Gui Concurrent REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent)
