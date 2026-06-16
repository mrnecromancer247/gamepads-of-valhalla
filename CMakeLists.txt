cmake_minimum_required(VERSION 3.21)
project(sdljoy_winmm C CXX)

# UE1 games are 32-bit; the naked-asm thunks are x86-only.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Configure for x86 (Win32). e.g.  cmake -A Win32 ..")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SDL2 -------------------------------------------------------------------
# Point SDL2_DIR at your SDL2 dev package, or install SDL2 so find_package works.
find_package(SDL2 REQUIRED)

# --- generate forwarders from the real winmm at configure time --------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(GEN_C   ${CMAKE_SOURCE_DIR}/src/forwarders_generated.c)
set(GEN_DEF ${CMAKE_SOURCE_DIR}/src/winmm.def)
add_custom_command(
  OUTPUT ${GEN_C} ${GEN_DEF}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/gen_forwarders.py
          --out ${CMAKE_SOURCE_DIR}/src
  COMMENT "Generating winmm forwarders from the system winmm.dll"
  VERBATIM)
add_custom_target(gen_forwarders DEPENDS ${GEN_C} ${GEN_DEF})

# --- the proxy dll (must be named winmm.dll) --------------------------------
add_library(winmm SHARED
  src/dllmain.cpp
  src/joystick.cpp
  src/config.cpp
  ${GEN_C})
add_dependencies(winmm gen_forwarders)

set_target_properties(winmm PROPERTIES
  OUTPUT_NAME "winmm"
  PREFIX ""
  LINK_FLAGS "/DEF:${GEN_DEF}")

target_include_directories(winmm PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(winmm PRIVATE ${SDL2_LIBRARIES})

# Copy the default ini next to the build output for convenience.
add_custom_command(TARGET winmm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/config/sdljoy.ini $<TARGET_FILE_DIR:winmm>)
