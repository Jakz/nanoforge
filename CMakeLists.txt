cmake_minimum_required(VERSION 3.16)

project(nanoforge)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable parallel compilation
if(MSVC)
  add_compile_options(/MP) # Enable parallel compilation for MSVC
endif()

# Set output directories for executables and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/intermediate)

# Set intermediate object files directory
set(CMAKE_OBJECT_PATH_MAX 260) # Ensure compatibility with long paths
set(CMAKE_OBJECT_OUTPUT_DIR ${CMAKE_BINARY_DIR}/intermediate)

# Specify the directory for CMake files
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_FILES_DIRECTORY ${CMAKE_BINARY_DIR}/cmake)

set(LIBS_DIR ${CMAKE_SOURCE_DIR}/libs)
include_directories(${LIBS_DIR}/raylib/include)
include_directories(${LIBS_DIR}/raylib-cpp/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# set SOURCE_FILES to all .cpp files in src directory
file(GLOB_RECURSE SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(nanoforge
  ${SOURCE_FILES}
  ${CMAKE_SOURCE_DIR}/src/renderer.cpp
)

if(APPLE)
  target_link_libraries(nanoforge PRIVATE
    ${LIBS_DIR}/raylib/lib/macos/libraylib.a
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework IOKit"
  )

  set(LLVM_HOME "/opt/homebrew/opt/llvm")

  target_compile_options(nanoforge PRIVATE -stdlib=libc++)
  #target_include_directories(nanoforge PRIVATE "${LLVM_HOME}/include/c++/v1")

  #target_link_options(nanoforge PRIVATE
  #  -stdlib=libc++
  #  -Wl,-rpath,"${LLVM_HOME}/lib/c++"
  #  -Wl,-rpath,"${LLVM_HOME}/lib/unwind"
  #  -Wl,-rpath,"${LLVM_HOME}/lib"
  #)

#  add_custom_command(TARGET cli POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy
#    ${LIBS_DIR}/blterm/lib/macos/libBearLibTerminal.dylib
#    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#  )

#  add_custom_command(TARGET cli POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy
#    ${CMAKE_SOURCE_DIR}/data/consola.ttf
#    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#  )
endif()