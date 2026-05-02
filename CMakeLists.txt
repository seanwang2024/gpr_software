cmake_minimum_required(VERSION 3.16)
project(MyQtApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# For Qt 6.8.3 with MinGW
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/mingw_64/lib/cmake" CACHE STRING "Qt6 prefix path")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts Quick QuickWidgets Quick3D)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
)

set(HEADERS
    include/MainWindow.h
)

set(RESOURCES
    resources.qrc
)

add_executable(MyQtApp ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(MyQtApp PRIVATE include)

target_link_libraries(MyQtApp PRIVATE Qt6::Core Qt6::Widgets Qt6::Charts Qt6::Quick Qt6::QuickWidgets Qt6::Quick3D)

if(WIN32)
    set_target_properties(MyQtApp PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
