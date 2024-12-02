cmake_minimum_required(VERSION 3.16)

project(ChatClient LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Multimedia)

add_executable(ChatClient
  main.cpp
  ChatClient.cpp
  ChatClient.h
)

target_link_libraries(ChatClient PRIVATE Qt5::Core Qt5::Widgets Qt5::Network Qt5::Multimedia)
