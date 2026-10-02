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
add_executable(contact_test tests/contact_test.cpp)
add_test(NAME contact_test COMMAND contact_test)
add_executable(ground_validate tests/ground_validate.cpp)
add_executable(ride_demo tests/ride_demo.cpp)
add_executable(sweep_cli tests/sweep_cli.cpp)
add_executable(ngp_col_check tests/ngp_col_check.cpp)
add_executable(bike_test tests/bike_test.cpp)
add_test(NAME bike_test COMMAND bike_test)
add_executable(bike_demo tests/bike_demo.cpp)

add_executable(integrator_test tests/integrator_test.cpp)
add_test(NAME integrator_test COMMAND integrator_test)

add_executable(rider_test tests/rider_test.cpp)
add_test(NAME rider_test COMMAND rider_test)
add_executable(rider_mesh_test tests/rider_mesh_test.cpp)
add_test(NAME rider_mesh_test COMMAND rider_mesh_test)

add_executable(nga_test tests/nga_test.cpp)
add_test(NAME nga_test COMMAND nga_test)
