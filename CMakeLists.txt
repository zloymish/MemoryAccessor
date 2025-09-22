cmake_minimum_required(VERSION 3.22)

project("MemoryAccessor" LANGUAGES CXX)

# Variables

set(CMAKE_MODULE_PATH 
  ${CMAKE_MODULE_PATH} 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CLASS_SRC_FILES 
  src/argvparser.cc 
  src/console.cc 
  src/hexviewer.cc 
  src/memoryaccessor.cc 
  src/processapi.cc 
  src/segmentinfo.cc
)
set(COMPILE_OPTS 
)
set(TESTING_COMPILE_OPTS
  -D PROJECT_TESTING
)
set(DEBUG_OPTS
  -O0 -g
)

# Trying to find dependencies

# Readline library

find_package(Readline)
include_directories(${Readline_INCLUDE_DIR})

# Doctest

find_path(DOCTEST_FOUND
  NAMES doctest/doctest.h
  HINTS /usr/include /usr/local/include
)

# clang-format

find_program(CLANG_FORMAT_BIN clang-format)

# MemoryAccessor executable

add_executable(MemoryAccessor 
  src/main.cc 
  ${CLASS_SRC_FILES}
)
target_link_libraries(MemoryAccessor 
  ${Readline_LIBRARY}
)
target_compile_options(MemoryAccessor PRIVATE 
  ${COMPILE_OPTS}
  $<$<CONFIG:Debug>: ${DEBUG_OPTS}>
)
target_link_options(MemoryAccessor PRIVATE
  $<$<CONFIG:Debug>: ${DEBUG_OPTS}>
)

# project_test executable

if(DOCTEST_FOUND)

add_executable(project_test 
  testing/project_test.cc 
  ${CLASS_SRC_FILES}
)
target_link_libraries(project_test 
  ${Readline_LIBRARY}
)
target_include_directories(project_test PUBLIC src)
target_compile_options(project_test PRIVATE 
  ${COMPILE_OPTS} 
  ${TESTING_COMPILE_OPTS}
  $<$<CONFIG:Debug>: ${DEBUG_OPTS}>
)
target_link_options(project_test PRIVATE 
  $<$<CONFIG:Debug>: ${DEBUG_OPTS}>
)

endif()

# ctest

if(DOCTEST_FOUND)

enable_testing()
add_test(NAME project_test COMMAND project_test --force-colors -d)

endif()

# Build and run tests

if(DOCTEST_FOUND)

add_custom_target(run-tests
  COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS project_test
)

endif()

# Documentation

find_package(Doxygen)

if(DOXYGEN_FOUND)

add_custom_target(doc-main
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/Doxyfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
  COMMENT "Generating main documentation with Doxygen"
)

add_custom_target(doc-testing
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/testing/Doxyfile-testing
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/testing
  COMMENT "Generating testing documentation with Doxygen"
)

add_custom_target(doc
  DEPENDS doc-main doc-testing
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

endif(DOXYGEN_FOUND)

# Formatting

if(CLANG_FORMAT_BIN)

set(FILES_TO_FORMAT
  ${CLASS_SRC_FILES}
  src/argvparser.h
  src/console.h 
  src/consolecommand.h 
  src/hexviewer.h 
  src/main.cc
  src/main.h
  src/memoryaccessor.h 
  src/processapi.h 
  src/segmentinfo.h
  testing/project_test.cc
  testing/project_test.h
)

add_custom_target(clang-format
  COMMAND clang-format -i ${FILES_TO_FORMAT}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Applying clang-format to sources"
)

endif()