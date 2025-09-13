cmake_minimum_required(VERSION 3.22)

project("MemoryAccessor" CXX)

set(CLASS_SRC_FILES src/argvparser.cc src/console.cc src/hexviewer.cc src/memoryaccessor.cc src/processapi.cc src/segmentinfo.cc)
set(COMPILE_OPTS -std=c++20)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(Readline)
include_directories(${Readline_INCLUDE_DIR})

add_executable(MemoryAccessor src/main.cc ${CLASS_SRC_FILES})
target_link_libraries(MemoryAccessor ${Readline_LIBRARY})
target_compile_options(MemoryAccessor PRIVATE ${COMPILE_OPTS})

add_executable(project_test testing/project_test.cc ${CLASS_SRC_FILES})
target_link_libraries(project_test ${Readline_LIBRARY})
target_include_directories(project_test PUBLIC src)
target_compile_options(project_test PRIVATE ${COMPILE_OPTS})
enable_testing()
add_test(NAME project_test COMMAND project_test --force-colors -d)