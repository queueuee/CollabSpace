cmake_minimum_required(VERSION 3.16)

project(ServerConsole LANGUAGES CXX)

# Включение автоматической обработки Qt
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Sql WebSockets)
# Создание исполняемого файла
add_executable(ServerConsole
  main.cpp
  ChatServer.h
  ChatServer.cpp
)

# Связывание библиотек
target_link_libraries(ServerConsole PRIVATE Qt${QT_VERSION_MAJOR}::Core
                                            Qt${QT_VERSION_MAJOR}::Network
                                            Qt${QT_VERSION_MAJOR}::Sql
                                            Qt${QT_VERSION_MAJOR}::WebSockets)

# Установка
include(GNUInstallDirs)
install(TARGETS ServerConsole
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
