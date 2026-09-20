cmake_minimum_required(VERSION 3.14)

project(assignment2_string LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 未指定构建类型时默认 Debug（保留调试信息，且不会定义 NDEBUG）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 打开常用告警，要求最终代码无警告
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# 可选：AddressSanitizer + UndefinedBehaviorSanitizer
#
#   cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
#   cmake --build build-asan -j
#   ctest --test-dir build-asan --output-on-failure
#
# 仅支持 GCC / Clang（Linux、macOS）：开启前会探测编译器是否支持
# -fsanitize=address,undefined，不支持则直接报错并给出替代方案，避免“以为开了
# sanitizer 其实没开”。手动指定标志的方法见 docs/build-and-test.md。
# ---------------------------------------------------------------------------
option(ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR
      "ENABLE_SANITIZERS 仅支持 GCC/Clang。MSVC 请改用 /fsanitize=address，"
      "详见 docs/build-and-test.md。")
  endif()

  # 必须做一次真实的编译 + 链接：只检查编译标志（check_cxx_compiler_flag）会因为
  # 链接时缺少 -fsanitize 而误判 GCC/Clang 不支持。
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
  check_cxx_source_compiles("int main() { return 0; }" COMPILER_SUPPORTS_ASAN_UBSAN)
  unset(CMAKE_REQUIRED_FLAGS)
  if(NOT COMPILER_SUPPORTS_ASAN_UBSAN)
    message(FATAL_ERROR
      "当前编译器不支持 -fsanitize=address,undefined，无法开启 ENABLE_SANITIZERS。\n"
      "请改用 GCC/Clang（例如 -DCMAKE_CXX_COMPILER=clang++），"
      "或参考 docs/build-and-test.md 手动指定可用的 sanitizer 标志。")
  endif()

  add_compile_options(
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all   # UBSan 报错立即终止，便于定位
  )
  add_link_options(-fsanitize=address,undefined)
endif()

include(CTest)  # 提供 enable_testing()

# ---------------------------------------------------------------------------
# 构建目标
#
# 如果你的实现拆分成多个 .cpp 文件，把新文件追加到 STRING_SOURCES 即可。
# ---------------------------------------------------------------------------
set(STRING_SOURCES
  src/my_string.cpp
  # 在这里追加你自己的 .cpp 文件
)

add_library(string_lib STATIC ${STRING_SOURCES})
target_include_directories(string_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(string_tests tests/string_tests.cpp)
target_link_libraries(string_tests PRIVATE string_lib)

if(BUILD_TESTING)
  add_test(NAME string_tests COMMAND string_tests)
endif()
