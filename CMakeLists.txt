cmake_minimum_required(VERSION 3.21)

#   cmake -DCMAKE_TOOLCHAIN_FILE=/SDK/share/cmake/arm_conf.cmake ..

project(clink C)

if(NOT CMAKE_CROSSCOMPILING)
  message(WARNING "PocketBook toolchain missing. "
                  "Use: -DCMAKE_TOOLCHAIN_FILE=/SDK/share/cmake/arm_conf.cmake")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if(DEFINED PB_INCLUDE_DIRECTORIES)
  include_directories(${PB_INCLUDE_DIRECTORIES})
endif()
if(DEFINED PB_LINK_DIRECTORIES)
  link_directories(${PB_LINK_DIRECTORIES})
endif()

if(DEFINED FREETYPE_INCLUDE_DIRS)
  include_directories(${FREETYPE_INCLUDE_DIRS})
endif()

set(_PB_SYSROOT "/SDK/usr/arm-obreey-linux-gnueabi/sysroot")
foreach(_inc
    "${_PB_SYSROOT}/usr/include"
    "${_PB_SYSROOT}/usr/include/freetype2"
    "/SDK/usr/include"
    "/SDK/usr/include/freetype2")
  if(EXISTS "${_inc}")
    include_directories("${_inc}")
  endif()
endforeach()

include_directories(${CMAKE_SOURCE_DIR})

add_executable(clink.app main.c board.c game.c)
target_compile_options(clink.app PRIVATE -std=gnu11 -Wall)

target_link_libraries(clink.app
  inkview
  freetype
  pthread
  m
  dl
)
