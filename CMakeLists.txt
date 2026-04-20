cmake_minimum_required(VERSION 3.16)
project(MyQtApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
)

set(HEADERS
    include/MainWindow.h
)

add_executable(MyQtApp ${SOURCES} ${HEADERS})

target_include_directories(MyQtApp PRIVATE include)

target_link_libraries(MyQtApp PRIVATE Qt6::Core Qt6::Widgets)

if(WIN32)
    set_target_properties(MyQtApp PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
