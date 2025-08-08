cmake_minimum_required(VERSION 3.5.0)
project(mediatransport4j-uwp VERSION 1.0.0 LANGUAGES CXX)

set(JAVA_HOME "$ENV{JAVA_HOME}")
set(CMAKE_CXX_STANDARD 17)

add_library(mediatransport4j-uwp SHARED
    uwp_impl.cpp
    uwp_impl.h
)

set_target_properties(mediatransport4j-uwp PROPERTIES
    VS_WINRT_COMPONENT TRUE
)

target_include_directories(mediatransport4j-uwp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${JAVA_HOME}/include
    PUBLIC ${JAVA_HOME}/include/win32
)