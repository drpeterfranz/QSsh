cmake_minimum_required(VERSION 3.14)

# set the project name and version
project(QSsh VERSION 1.0 LANGUAGES CXX)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(botan 3.6 REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

# qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (MSVC)
    add_compile_options(/EHsc)
endif()

add_subdirectory(src)
add_subdirectory(examples)

# INSTALL RULES
install(EXPORT QSsh-targets
  FILE QSshTargets.cmake
  NAMESPACE QSsh::
  DESTINATION share/QSsh/cmake
)


install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmakeFindModules/QSshConfig.cmake"
        DESTINATION share/QSsh/cmake)
