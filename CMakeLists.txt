cmake_minimum_required(VERSION 3.16)
project(labirintos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(labirintos_nucleo STATIC
  src/maze.cpp
  src/generators.cpp
  src/solvers.cpp
  src/render.cpp
  src/png.cpp
  src/raster.cpp
  src/stats.cpp
)
target_include_directories(labirintos_nucleo PUBLIC src)

add_executable(labirinto src/main.cpp)
target_link_libraries(labirinto PRIVATE labirintos_nucleo)

add_executable(testes
  tests/main.cpp
  tests/test_geradores.cpp
  tests/test_resolvedores.cpp
  tests/test_png.cpp
  tests/test_desenho.cpp
)
target_include_directories(testes PRIVATE tests)
target_link_libraries(testes PRIVATE labirintos_nucleo)

enable_testing()
add_test(NAME unidade COMMAND testes)
add_test(NAME integracao
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/integracao.sh $<TARGET_FILE:labirinto>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
