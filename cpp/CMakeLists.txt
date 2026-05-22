# SPDX-License-Identifier: MIT
# Copyright (c) 2026 SPZ Gatekeeper Contributors

cmake_minimum_required(VERSION 3.16)

project(spz_gatekeeper
  VERSION 2.0.0
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
option(SPZ_GATEKEEPER_BUILD_WASM
  "Build WebAssembly target (Emscripten only)"
  OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(SPZ_GATEKEEPER_BUILD_WASM ON)
endif()

if(SPZ_GATEKEEPER_BUILD_WASM AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  message(FATAL_ERROR "SPZ_GATEKEEPER_BUILD_WASM requires Emscripten toolchain (use emcmake).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SPZ_GATEKEEPER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(SPZ_GATEKEEPER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(SPZ_GATEKEEPER_VERSION_PATCH ${PROJECT_VERSION_PATCH})

add_library(spz_gatekeeper_zlib INTERFACE)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_compile_options(spz_gatekeeper_zlib INTERFACE "-sUSE_ZLIB=1")
  target_link_options(spz_gatekeeper_zlib INTERFACE "-sUSE_ZLIB=1")
else()
  find_package(ZLIB REQUIRED)
  if(ZLIB_VERSION_STRING VERSION_LESS "1.3.1")
    message(WARNING "System zlib ${ZLIB_VERSION_STRING} < 1.3.1. CI enforces 1.3.1 via runner pre-install.")
  endif()
  target_link_libraries(spz_gatekeeper_zlib INTERFACE ZLIB::ZLIB)
endif()

# zstd: required for SPZ v4 ZSTD decompression (v2.0.2).
# Locked at v1.5.6 with SHA256 verification per supply-chain defense policy.
set(SPZ_GATEKEEPER_ZSTD_VERSION "1.5.6")
set(SPZ_GATEKEEPER_ZSTD_SHA256
  "8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1")
set(SPZ_GATEKEEPER_ZSTD_URL
  "https://github.com/facebook/zstd/releases/download/v${SPZ_GATEKEEPER_ZSTD_VERSION}/zstd-${SPZ_GATEKEEPER_ZSTD_VERSION}.tar.gz")

set(SPZ_GATEKEEPER_CORE_SOURCES
  src/audit_summary.cc
  src/extension_spec_registry.cc
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
target_link_libraries(spz_gatekeeper_core PUBLIC spz_gatekeeper_zlib)

# zstd v1.5.6: link directly to core (no INTERFACE middleman).
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  include(FetchContent)
  FetchContent_Declare(zstd URL ${SPZ_GATEKEEPER_ZSTD_URL}
    URL_HASH SHA256=${SPZ_GATEKEEPER_ZSTD_SHA256})
  set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(zstd)
  target_include_directories(spz_gatekeeper_core PUBLIC ${zstd_SOURCE_DIR}/lib)
  target_link_libraries(spz_gatekeeper_core PUBLIC libzstd_static)
else()
  find_package(zstd ${SPZ_GATEKEEPER_ZSTD_VERSION} QUIET)
  if(zstd_FOUND AND TARGET zstd::libzstd_static)
    target_link_libraries(spz_gatekeeper_core PUBLIC zstd::libzstd_static)
  elseif(zstd_FOUND AND TARGET zstd::libzstd_shared)
    target_link_libraries(spz_gatekeeper_core PUBLIC zstd::libzstd_shared)
  else()
    find_library(ZSTD_LIBRARY zstd)
    find_path(ZSTD_INCLUDE_DIR zstd.h)
    if(ZSTD_LIBRARY AND ZSTD_INCLUDE_DIR)
      target_link_libraries(spz_gatekeeper_core PUBLIC ${ZSTD_LIBRARY})
      target_include_directories(spz_gatekeeper_core PUBLIC ${ZSTD_INCLUDE_DIR})
    else()
      include(FetchContent)
      FetchContent_Declare(zstd URL ${SPZ_GATEKEEPER_ZSTD_URL}
        URL_HASH SHA256=${SPZ_GATEKEEPER_ZSTD_SHA256})
      set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
      set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
      set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      FetchContent_MakeAvailable(zstd)
      target_include_directories(spz_gatekeeper_core PUBLIC ${zstd_SOURCE_DIR}/lib)
      target_link_libraries(spz_gatekeeper_core PUBLIC libzstd_static)
    endif()
  endif()
endif()

target_compile_options(spz_gatekeeper_core PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    -Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4 /permissive->
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
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
endif()

if(SPZ_GATEKEEPER_BUILD_WASM)
  set(SPZ_GATEKEEPER_WASM_SITE_DIR "${CMAKE_CURRENT_BINARY_DIR}/site")

  add_executable(spz_gatekeeper-wasm src/wasm_main.cc)
  target_link_libraries(spz_gatekeeper-wasm PRIVATE spz_gatekeeper_core)
  target_link_options(spz_gatekeeper-wasm PRIVATE
    "-O3"
    "--bind"
    "--no-entry"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createSpzGatekeeperModule"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sENVIRONMENT=web"
    "-sEXPORT_ES6=1"
    "-sSINGLE_FILE=1"
    "-sFILESYSTEM=0"
  )
  set_target_properties(spz_gatekeeper-wasm PROPERTIES
    OUTPUT_NAME "spz_gatekeeper_wasm"
    RUNTIME_OUTPUT_DIRECTORY "${SPZ_GATEKEEPER_WASM_SITE_DIR}"
  )

  add_custom_command(
    OUTPUT "${SPZ_GATEKEEPER_WASM_SITE_DIR}/index.html"
           "${SPZ_GATEKEEPER_WASM_SITE_DIR}/spz_gatekeeper.js"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPZ_GATEKEEPER_WASM_SITE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../web/index.html"
            "${SPZ_GATEKEEPER_WASM_SITE_DIR}/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../web/spz_gatekeeper.js"
            "${SPZ_GATEKEEPER_WASM_SITE_DIR}/spz_gatekeeper.js"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../web/index.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/../web/spz_gatekeeper.js"
    COMMENT "Copy web entrypoint and browser audit wrapper into WASM site directory"
    VERBATIM
  )

  add_custom_target(spz_gatekeeper_wasm_site ALL
    DEPENDS spz_gatekeeper-wasm
            "${SPZ_GATEKEEPER_WASM_SITE_DIR}/index.html"
            "${SPZ_GATEKEEPER_WASM_SITE_DIR}/spz_gatekeeper.js"
  )
endif()

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

if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
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
  spz_gatekeeper_add_test(extension_spec_registry_test tests/extension_spec_registry_test.cc)
  spz_gatekeeper_add_test(registry_cli_test tests/registry_cli_test.cc)
  spz_gatekeeper_add_test(gen_fixture_test tests/gen_fixture_test.cc)
  spz_gatekeeper_add_test(compat_check_test tests/compat_check_test.cc)
  spz_gatekeeper_add_test(audit_summary_test tests/audit_summary_test.cc)
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
message(STATUS "Build WASM: ${SPZ_GATEKEEPER_BUILD_WASM}")
message(STATUS "Sync compile_commands: ${SPZ_GATEKEEPER_SYNC_COMPILE_COMMANDS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")
message(STATUS "")
