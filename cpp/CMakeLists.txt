# SPDX-License-Identifier: MIT
# Copyright (c) 2026 SPZ Gatekeeper Contributors

cmake_minimum_required(VERSION 3.16)

project(spz_gatekeeper
  VERSION 1.0.0
  DESCRIPTION "SPZ Gatekeeper - SPZ format legality checker"
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CTest)

option(SPZ_GATEKEEPER_SYNC_COMPILE_COMMANDS
  "Sync compile_commands.json into the source tree for local IDE indexing"
  OFF)
option(SPZ_GATEKEEPER_BUILD_BENCHMARK_TESTS
  "Build benchmark-oriented tests"
  OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SPZ_GATEKEEPER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(SPZ_GATEKEEPER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(SPZ_GATEKEEPER_VERSION_PATCH ${PROJECT_VERSION_PATCH})

find_package(ZLIB REQUIRED)

set(SPZ_GATEKEEPER_CORE_SOURCES
  src/json_min.cc
  src/report.cc
  src/spz.cc
  src/tlv.cc
)

# Core library: L2 SPZ validation engine.
add_library(spz_gatekeeper_core ${SPZ_GATEKEEPER_CORE_SOURCES})

target_include_directories(spz_gatekeeper_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extensions>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/extensions>
)
target_link_libraries(spz_gatekeeper_core PUBLIC ZLIB::ZLIB)

target_compile_options(spz_gatekeeper_core PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    -Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4 /permissive->
)

add_executable(spz_gatekeeper src/main.cc)
target_link_libraries(spz_gatekeeper PRIVATE spz_gatekeeper_core)

if(MSVC)
  target_compile_options(spz_gatekeeper PRIVATE /Zc:strictStrings-)
endif()

set_target_properties(spz_gatekeeper PROPERTIES
  VERSION ${PROJECT_VERSION}
  OUTPUT_NAME "spz_gatekeeper"
)

install(TARGETS spz_gatekeeper
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY extensions/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/extensions)

if(SPZ_GATEKEEPER_SYNC_COMPILE_COMMANDS
   AND CMAKE_EXPORT_COMPILE_COMMANDS
   AND CMAKE_GENERATOR MATCHES "Ninja|Unix Makefiles|MinGW Makefiles")
  add_custom_target(spz_gatekeeper_sync_compile_commands
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
            ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
    COMMENT "Sync compile_commands.json to source tree for IDE indexing"
  )
endif()

if(BUILD_TESTING)
  function(spz_gatekeeper_add_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE spz_gatekeeper_core ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${target} COMMAND ${target})
  endfunction()

  add_test(NAME spz_gatekeeper_self_test COMMAND spz_gatekeeper --self-test)

  add_executable(spz_gatekeeper_test tests/spz_gatekeeper_test.cc)
  target_link_libraries(spz_gatekeeper_test PRIVATE spz_gatekeeper_core)
  add_test(NAME spz_gatekeeper_unit_test COMMAND spz_gatekeeper_test)

  spz_gatekeeper_add_test(extension_validator_test tests/extension_validator_test.cc)
  spz_gatekeeper_add_test(adobe_extension_test tests/adobe_extension_test.cc)
  spz_gatekeeper_add_test(validator_registry_test tests/validator_registry_test.cc)
  spz_gatekeeper_add_test(tlv_multi_record_test tests/tlv_multi_record_test.cc)
  spz_gatekeeper_add_test(extension_integration_test tests/extension_integration_test.cc ZLIB::ZLIB)
  spz_gatekeeper_add_test(sh_quantization_test tests/sh_quantization_test.cc)

  if(SPZ_GATEKEEPER_BUILD_BENCHMARK_TESTS)
    spz_gatekeeper_add_test(performance_benchmark_test tests/performance_benchmark_test.cc)
  endif()
endif()

message(STATUS "")
message(STATUS "========================================")
message(STATUS "SPZ Gatekeeper v${PROJECT_VERSION}")
message(STATUS "========================================")
if(CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Configurations: ${CMAKE_CONFIGURATION_TYPES}")
else()
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "Build testing: ${BUILD_TESTING}")
message(STATUS "Benchmark tests: ${SPZ_GATEKEEPER_BUILD_BENCHMARK_TESTS}")
message(STATUS "Sync compile_commands: ${SPZ_GATEKEEPER_SYNC_COMPILE_COMMANDS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")
message(STATUS "")
