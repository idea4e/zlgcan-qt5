cmake_minimum_required(VERSION 3.14)

set(target "qtzlgcanbus")
project(${target} LANGUAGES CXX)
set(CMAKE_DEBUG_POSTFIX "d")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Gui LinguistTools SerialBus)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui LinguistTools SerialBus)

add_library(${target} SHARED
    main.cpp
    main.h
    zlgcanbackend.cpp
    zlgcanbackend.h
    zlgcanbackend_p.cpp
    zlgcanbackend_p.h
    zlgcan.qrc
)

target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_link_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/lib/zlgcan)

target_link_libraries(${target} PRIVATE Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::SerialBus zlgcan)

target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
