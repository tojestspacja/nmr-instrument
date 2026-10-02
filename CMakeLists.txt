cmake_minimum_required(VERSION 3.20)
project(nmr_instrument LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# pulse-program IR + compiler: no dependencies, also compiled into the firmware
add_library(pulse STATIC pulse/src/program.cpp pulse/src/compiler.cpp pulse/src/backend.cpp)
target_include_directories(pulse PUBLIC pulse/include)

# physics + DSP core
add_library(nmrcore STATIC physics/src/field.cpp physics/src/dsp.cpp physics/src/probe.cpp physics/src/sim.cpp
                           physics/src/pipeline.cpp)
target_include_directories(nmrcore PUBLIC physics/include generated)
target_link_libraries(nmrcore PUBLIC pulse)

# C ABI as a shared library for Python (ctypes)
add_library(nmr SHARED physics/src/capi.cpp)
target_link_libraries(nmr PRIVATE nmrcore)
if(MINGW)
  target_link_options(nmr PRIVATE -static -static-libgcc -static-libstdc++)
endif()

enable_testing()
foreach(t test_bloch test_field test_dsp test_pulse test_sim test_backend test_iq)
  add_executable(${t} physics/tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE nmrcore)
  target_include_directories(${t} PRIVATE physics/tests)
  if(MINGW)
    target_link_options(${t} PRIVATE -static)
  endif()
  add_test(NAME ${t} COMMAND ${t})
endforeach()

add_executable(bench_core physics/benchmarks/bench_core.cpp)
target_link_libraries(bench_core PRIVATE nmrcore)
if(MINGW)
  target_link_options(bench_core PRIVATE -static)
endif()
