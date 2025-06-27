cmake_minimum_required(VERSION 3.16)

project(Client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt5 REQUIRED COMPONENTS Core
                                     Widgets
                                     Network
                                     Multimedia
                                     Sql
                                     WebSockets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.0
                                gstreamer-video-1.0
                                gstreamer-plugins-base-1.0)

include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})
add_definitions(${GST_CFLAGS_OTHER})

# Определение исходных файлов
set(PROJECT_SOURCES
    main.cpp
    clientmain.cpp
    clientmain.h
    clientmain.ui
)

# Создание исполняемого файла
add_executable(Client ${PROJECT_SOURCES}
    icons.qrc
    authorization.h authorization.cpp authorization.ui
    systemmanager.h systemmanager.cpp
    networkmanager.h networkmanager.cpp
    serversettings.h serversettings.cpp serversettings.ui


)

# Привязка библиотек к целевой программе
target_link_libraries(Client Qt5::Core Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::WebSockets ${GST_LIBRARIES})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.

set_target_properties(Client PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Client)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "client")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "you@example.com")
set(CPACK_PACKAGE_DESCRIPTION "Qt-based Client Application")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a, libqt5widgets5, libqt5network5, libqt5multimedia5, libqt5websockets5, gstreamer1.0-plugins-base, gstreamer1.0-tools")

include(CPack)
