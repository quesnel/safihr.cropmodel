cmake_minimum_required(VERSION 2.8.8)
project(safihr.cropmodel CXX)
enable_testing()

set(MODEL_MAJOR 0)
set(MODEL_MINOR 1)
set(MODEL_PATCH 0)
set(MODEL_NAME "${PROJECT_NAME}-${MODEL_MAJOR}.${MODEL_MINOR}.${MODEL_PATCH}")
set(VLE_NAME "${PROJECT_NAME}-${MODEL_MAJOR}.${MODEL_MINOR}")

if (CMAKE_BUILD_TYPE STREQUAL "")
  SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build" FORCE)
endif ()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")
endif ()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH})

##
## Modules
##

include(CheckIncludeFileCXX)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CMakeDetermineCCompiler)

##
## Check libraries with pkgconfig
##

find_package(VLE REQUIRED)

##
## Check VLE's packages
##

set(VleCheckPackage_DIR "${CMAKE_SOURCE_DIR}/cmake")
find_package(VleCheckPackage REQUIRED)
set(VLE_ABI_VERSION 1.1)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREAD ON)
find_package(Boost COMPONENTS unit_test_framework date_time)

if (Boost_UNIT_TEST_FRAMEWORK_FOUND)
  set(VLE_HAVE_UNITTESTFRAMEWORK 1 CACHE INTERNAL "" FORCE)
endif ()

##
## Generate the doxygen
##

set(WITH_DOC OFF)
find_package(Doxygen)
if (WITH_DOC AND DOXYGEN)
  set(DOXYGEN_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")
  set(DOXYGEN_OUTPUT_MODELING_DIR "${PROJECT_BINARY_DIR}/doxygen/modeling")
  set(DOXYGEN_OUTPUT_SOURCE_DIR "${PROJECT_BINARY_DIR}/doxygen/sources")
  configure_file("cmake/doxygen-modeling.conf.in"
    "${PROJECT_BINARY_DIR}/doxygen-modeling.conf")
  configure_file("cmake/doxygen-sources.conf.in"
    "${PROJECT_BINARY_DIR}/doxygen-sources.conf")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/doxygen")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/doxygen/modeling")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/doxygen/sources")

  add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/doxygen/modeling/index.html"
    DEPENDS "${PROJECT_BINARY_DIR}/doxygen-modeling.conf"
    COMMAND "${DOXYGEN}"
    ARGS "${PROJECT_BINARY_DIR}/doxygen-modeling.conf")

  add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/doxygen/sources/index.html"
    DEPENDS "${PROJECT_BINARY_DIR}/doxygen-sources.conf"
    COMMAND "${DOXYGEN}"
    ARGS "${PROJECT_BINARY_DIR}/doxygen-sources.conf")

  add_custom_target(doc_modeling ALL DEPENDS
    "${PROJECT_BINARY_DIR}/doxygen-modeling.conf"
    "${PROJECT_BINARY_DIR}/doxygen/modeling/index.html" VERBATIM)
  add_custom_target(doc_sources ALL DEPENDS
    "${PROJECT_BINARY_DIR}/doxygen-sources.conf"
    "${PROJECT_BINARY_DIR}/doxygen/sources/index.html" VERBATIM)

  install(DIRECTORY "${PROJECT_BINARY_DIR}/doxygen/modeling/html" DESTINATION
    "doc/html/modeling")
  install(DIRECTORY "${PROJECT_BINARY_DIR}/doxygen/sources/html" DESTINATION
    "doc/html/sources")
endif ()

##
## Define function to simplify the definition of simulations plugins.
##

function(DeclareDevsDynamics name sources)
  add_library(${name} MODULE ${sources})
  target_link_libraries(${name} ${VLE_LIBRARIES})
  install(TARGETS ${name}
    RUNTIME DESTINATION plugins/simulator
    LIBRARY DESTINATION plugins/simulator)
endfunction(DeclareDevsDynamics name sources)

##
## Subdirectory
##

add_subdirectory(data)
add_subdirectory(doc)
add_subdirectory(exp)
add_subdirectory(src)

set(WITH_TEST OFF)
if (WITH_TEST AND Boost_UNIT_TEST_FRAMEWORK_FOUND)
  add_subdirectory(test)
endif ()

##
## CPack configuration
##

install(FILES Authors.txt Description.txt License.txt News.txt Readme.txt
  DESTINATION .)

include(CMakeCPack.cmake)
