cmake_minimum_required(VERSION 3.20)
project(verity_graph VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VERITY_SANITIZE "Build tests/CLI with AddressSanitizer (UBSan where the platform supports it)" OFF)
option(VERITY_WERROR "Treat compiler warnings as errors" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(VERITY_WERROR)
    add_compile_options(-Werror)
  endif()
  if(VERITY_SANITIZE)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    if(NOT WIN32)
      # UBSan is not fully supported by the clang windows-msvc target;
      # enable it on Linux/macOS, document the limitation for Windows.
      add_compile_options(-fsanitize=undefined)
      add_link_options(-fsanitize=undefined)
    endif()
  endif()
elseif(MSVC)
  add_compile_options(/W4 /permissive- /EHsc /utf-8)
  if(VERITY_WERROR)
    add_compile_options(/WX)
  endif()
  if(VERITY_SANITIZE)
    add_compile_options(/fsanitize=address)
  endif()
endif()

# ---------------------------------------------------------------------------
# Header-only handwritten algorithm libraries
# ---------------------------------------------------------------------------
add_library(verity_ds INTERFACE)
target_include_directories(verity_ds INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(verity_dm INTERFACE)
target_include_directories(verity_dm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# verity_core: model + util + io + engine (compiled)
# ---------------------------------------------------------------------------
add_library(verity_core STATIC
  src/verity/util/Logger.cpp
  src/verity/model/Node.cpp
  src/verity/model/ClaimNode.cpp
  src/verity/model/NodeStore.cpp
  src/verity/io/JsonLoader.cpp
  src/verity/engine/GraphBuilder.cpp
  src/verity/engine/VerityEngine.cpp
)
target_include_directories(verity_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Vendored third-party headers are SYSTEM includes: their warnings are not
# ours to fix (doctest uses __COUNTER__, a C2y extension under clang 23).
target_include_directories(verity_core SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(verity_core PUBLIC verity_ds verity_dm)
find_package(Threads REQUIRED)
target_link_libraries(verity_core PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# verity_cli: the `verity` executable
# ---------------------------------------------------------------------------
add_executable(verity_cli app/main.cpp)
set_target_properties(verity_cli PROPERTIES OUTPUT_NAME verity)
target_link_libraries(verity_cli PRIVATE verity_core)

# ---------------------------------------------------------------------------
# tests (doctest, single combined runner)
# ---------------------------------------------------------------------------
enable_testing()

set(VERITY_TEST_SOURCES
  tests/test_main.cpp
  tests/oop/test_model.cpp
  tests/oop/test_binding.cpp
  tests/oop/test_nodestore.cpp
  tests/ds/test_ds_core.cpp
  tests/ds/test_hashtable.cpp
  tests/ds/test_graph.cpp
  tests/dm/test_numtheory.cpp
  tests/dm/test_sets.cpp
  tests/dm/test_counting.cpp
  tests/io/test_jsonloader.cpp
  tests/integration/test_pipeline.cpp
)

add_executable(verity_tests ${VERITY_TEST_SOURCES})
target_include_directories(verity_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(verity_tests SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(verity_tests PRIVATE verity_core)
add_test(NAME verity_all_tests COMMAND verity_tests)

# ---------------------------------------------------------------------------
# Windows + clang sanitizer support: the MSVC STL requests its ASan runtime
# (stl_asan.lib) via #pragma comment(lib, ...). clang does not add the MSVC
# toolset lib dir automatically, so locate a toolset that ships it.
# ---------------------------------------------------------------------------
if(VERITY_SANITIZE AND WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(_VERITY_MSVC_STL_LIB "")
  foreach(_root
      "$ENV{VERITY_MSVC_ROOT}/VC/Tools/MSVC"
      "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"
      "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC"
      "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC")
    if(EXISTS "${_root}")
      file(GLOB _toolsets "${_root}/*")
      list(SORT _toolsets ORDER DESCENDING)
      foreach(_ts ${_toolsets})
        if(EXISTS "${_ts}/lib/x64/stl_asan.lib")
          set(_VERITY_MSVC_STL_LIB "${_ts}/lib/x64")
          break()
        endif()
      endforeach()
    endif()
    if(_VERITY_MSVC_STL_LIB)
      break()
    endif()
  endforeach()
  if(_VERITY_MSVC_STL_LIB)
    message(STATUS "ASan: using MSVC STL runtime at ${_VERITY_MSVC_STL_LIB}")
    target_link_directories(verity_tests BEFORE PRIVATE "${_VERITY_MSVC_STL_LIB}")
    target_link_directories(verity_cli BEFORE PRIVATE "${_VERITY_MSVC_STL_LIB}")
    # Also register the clang ASan runtime DLL directory on the test entry so
    # ctest works without the user adding LLVM paths to PATH manually.
    get_filename_component(_clang_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB _clang_rt_dirs "${_clang_bin_dir}/../lib/clang/*/lib/windows")
    if(_clang_rt_dirs)
      list(GET _clang_rt_dirs 0 _clang_rt_dir)
      message(STATUS "ASan: runtime DLL dir ${_clang_rt_dir}")
      set_tests_properties(verity_all_tests PROPERTIES
        ENVIRONMENT "PATH=${_clang_rt_dir};$ENV{PATH}")
    endif()
  else()
    message(WARNING
      "VERITY_SANITIZE=ON on Windows with clang requires stl_asan.lib from an "
      "MSVC toolset (set VERITY_MSVC_ROOT to your VS installation). Building "
      "without sanitizers is fine; sanitizer runs will be labeled Not run.")
  endif()
endif()
