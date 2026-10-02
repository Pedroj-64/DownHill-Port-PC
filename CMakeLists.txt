# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(downhill CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
add_executable(dhview src/main.cpp)
target_link_libraries(dhview PRIVATE SDL3::SDL3 OpenGL::GL)

# Empaquetado: `cpack -G DEB` (Linux) / ZIP (Windows, vía mingw-w64 o MSVC). No incluye datos del juego:
# el usuario provee su propia ISO y la app extrae los assets en el primer arranque.
install(TARGETS dhview RUNTIME DESTINATION bin)
set(CPACK_PACKAGE_NAME downhill-port)
set(CPACK_PACKAGE_VERSION 0.0.1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Community native PC port of Downhill Domination / Port nativo comunitario de Downhill Domination")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "dev")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

enable_testing()
add_executable(ground_test tests/ground_test.cpp)
add_test(NAME ground_test COMMAND ground_test)
add_executable(ground_validate tests/ground_validate.cpp)
add_executable(ride_demo tests/ride_demo.cpp)
