cmake_minimum_required(VERSION 3.22)

project("MemoryAccessor" CXX)

# Variables

set(CLASS_SRC_FILES 
  src/argvparser.cc 
  src/console.cc 
  src/hexviewer.cc 
  src/memoryaccessor.cc 
  src/processapi.cc 
  src/segmentinfo.cc
)
set(COMPILE_OPTS 
  -std=c++20
)
set(TESTING_COMPILE_OPTS
  -D PROJECT_TESTING
)
set(DEBUG_OPTS
  -g
)

# Readline library

set(CMAKE_MODULE_PATH 
  ${CMAKE_MODULE_PATH} 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)
find_package(Readline)
include_directories(${Readline_INCLUDE_DIR})

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

# ctest

enable_testing()
add_test(NAME project_test COMMAND project_test --force-colors -d)