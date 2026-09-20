# 构建、测试与内存检查指南

本文档面向学生，适用于 **Linux（GCC / Clang）** 与 **macOS（AppleClang / Homebrew Clang）**。
所有命令均在仓库根目录（`assignment2-string/`）下执行。

---

## 0. 命令一览

```bash
# 默认构建（已开启 ASan + UBSan）+ 测试
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

# 不带 sanitizer 的普通构建（需要时）
cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
cmake --build build-plain -j
ctest --test-dir build-plain --output-on-failure
```

---

## 1. 环境准备

### Linux

```bash
# 检查工具链
g++ --version          # 建议 GCC ≥ 9；或 clang++ --version，建议 Clang ≥ 10
cmake --version        # 需要 ≥ 3.14

# Debian / Ubuntu
sudo apt install build-essential cmake gdb

# Fedora / RHEL
sudo dnf install gcc-c++ cmake gdb
```

### macOS

```bash
xcode-select --install      # 安装 AppleClang 与命令行工具
brew install cmake          # 或从 cmake.org 下载安装包
clang++ --version
cmake --version
```

CMake 会自动选择系统默认编译器；想换编译器时在配置阶段指定
`-DCMAKE_CXX_COMPILER=clang++`（或 `g++`）即可。

---

## 2. 构建与测试

### 2.1 配置与编译

```bash
cmake -S . -B build
cmake --build build -j
```

- `-S .` 指定源码目录，`-B build` 指定构建目录（生成物都放在 `build/`，不会污染源码）；
- **默认就开启 ASan + UBSan**（详见第 3 节）：配置阶段会探测编译器支持，
  支持则加上 sanitizer 标志；不支持则直接报错，可加 `-DENABLE_SANITIZERS=OFF` 关闭；
- 工程已统一开启 `-Wall -Wextra -Wpedantic`，完成前应保证没有任何警告；
- 未指定 `CMAKE_BUILD_TYPE` 时默认 `Debug`（保留调试信息，便于 ASan 报告定位到行号）；
- 若你的实现拆成了多个 `.cpp`，把它们加入 `CMakeLists.txt` 里的 `STRING_SOURCES` 列表。

### 2.2 首次构建的“预期失败”

拿到仓库时 `src/my_string.cpp` 是空的，因此链接 `string_tests` 时会出现类似报错：

```text
undefined reference to `String::String()'
undefined reference to `String::size() const'
...
collect2: error: ld returned 1 exit status
```

这**不是环境问题**，而是提醒你还有函数没有实现。每完成一部分实现，重新执行
`cmake --build build -j` 即可；全部实现后此错误自然消失。

### 2.3 运行测试

```bash
# 方式一：通过 CTest（推荐，等价于验收命令）
ctest --test-dir build --output-on-failure

# 方式二：直接运行测试程序
./build/string_tests
```

测试程序逐条输出用例结果，形如：

```text
[ RUN      ] default_constructed_empty
[       OK ] default_constructed_empty
...
[ RUN      ] insert_basic
tests/string_tests.cpp:372: CHECK failed: c_str(s) == expected (actual="aXYbcd", expected="abXYcd")
[   FAILED ] insert_basic

checks: 412, failures: 1
TESTS FAILED
```

- 失败信息给出 **文件:行号、失败的表达式、实际值与期望值**，直接定位到出错语义；
- 退出码：全部通过为 `0`，有失败为 `1`（CTest 依赖退出码判定成败）；
- 在 CMake < 3.20 上 `ctest --test-dir` 会报 `unrecognized option`，改用：

```bash
cd build && ctest --output-on-failure
```

### 2.4 清理与完全重建

改了 `CMakeLists.txt` 或出现奇怪的缓存问题时：

```bash
rm -rf build && cmake -S . -B build && cmake --build build -j
```

### 2.5 只调试某几条用例

测试程序在 `main()` 中按顺序调用 `run("名字", 函数)`。调试时可以临时把不需要的
`run(...)` 行注释掉，只保留目标用例，最后**完成后务必还原** `tests/string_tests.cpp`。

---

## 3. AddressSanitizer + UndefinedBehaviorSanitizer

### 3.1 默认开启与关闭方法

**ASan + UBSan 默认开启**，所以第 2 节的 `build/` 就是内存检查版本：

```bash
cmake -S . -B build              # 默认即带 ASan+UBSan
cmake --build build -j
ctest --test-dir build --output-on-failure
```

显式写出开关（与默认行为等价，便于在命令里看清意图）或使用独立目录：

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

确实需要不带 sanitizer 的普通构建时：

```bash
cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
cmake --build build-plain -j
ctest --test-dir build-plain --output-on-failure
```

开启后工程会为**编译与链接**都加上：

| 标志 | 作用 |
| --- | --- |
| `-fsanitize=address` | 检查越界读写、use-after-free、double free、内存泄漏等 |
| `-fsanitize=undefined` | 检查有符号溢出、空指针解引用、越界移位、错误的类型转换等 UB |
| `-fno-omit-frame-pointer` | 保留帧指针，让报告能打印完整调用栈 |
| `-fno-sanitize-recover=all` | UB 一旦发生立即终止进程，而不是“打印后继续” |

配置阶段会先用 `check_cxx_source_compiles` 做一次**真实的编译 + 链接**探测（只检查
编译标志会因链接时缺少 `-fsanitize` 而误判，因此不能简单地用
`check_cxx_compiler_flag`）：

- 编译器支持 → 正常生成；
- 编译器不支持（或使用 MSVC）→ CMake **直接报错**并提示改用 GCC/Clang 或加
  `-DENABLE_SANITIZERS=OFF`，不会静默退化成普通构建，避免“以为开了 sanitizer
  其实没开”。

### 3.2 手动指定标志（备用方案）

不想用（或改不了）上面的选项时，可以绕过 CMake 选项手动指定（记得同时用
`-DENABLE_SANITIZERS=OFF` 关掉默认的探测与报错）：

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

如果工具链对 UBSan 支持不完整，可以退化为只开 ASan：

```bash
cmake -S . -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
```

### 3.3 运行与报告解读

**内存泄漏（Linux）**：GCC/Clang 的 ASan 在 Linux 上默认开启 LeakSanitizer；
也可显式打开：

```bash
ASAN_OPTIONS=detect_leaks=1 ./build-asan/string_tests
```

出现泄漏时报告形如：

```text
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 17 byte(s) in 1 object(s) allocated from:
    #0 0x... in operator new[](unsigned long)
    #1 0x... in String::String(char const*) src/my_string.cpp:57
```

含义：某次 `new[]` 分配的内存没有任何 `delete[]` 释放。常见原因：

- 析构函数漏写 `delete[]`；
- 扩容时直接覆盖了旧指针，未先释放旧缓冲区；
- 复制赋值走“先释放、再分配”，中途原指针丢失；
- 移动后源对象未置空，其析构函数又释放了一次（通常表现为 double free 或 UAF）。

**常见 ASan 报告关键词**：

| 关键词 | 含义 |
| --- | --- |
| `heap-buffer-overflow` | 读写了缓冲区边界之外（常见于手写循环的 `<=` / 差一错误） |
| `heap-use-after-free` | 使用了已释放的内存（常见于自赋值/自插入/移动后继续使用） |
| `attempting double-free` | 同一块内存释放两次（常与移动后未置空有关） |
| `LeakSanitizer: detected memory leaks` | 有分配未释放 |

**UBSan 报告**形如 `runtime error: signed integer overflow`、
`runtime error: null pointer passed as argument` 等；由于我们加了
`-fno-sanitize-recover=all`，出现后会立即中止，先修复第一条报错再看后续。

**macOS 说明**：AppleClang 的 ASan/UBSan 可用于检测越界、UAF、double free 与 UB，
但 **LeakSanitizer 在 macOS 上支持有限**，ASan 通常不会报告内存泄漏。
因此 macOS 同学请把重点放在“无 ASan/UBSan 报错”上；如确实需要查泄漏，
可以用 Xcode 的 Instruments（Leaks）工具，或到 Linux/WSL2 下跑一遍上面的泄漏检测。

**MSVC（仅作参考）**：本课程不要求 Windows 原生环境，建议使用 WSL2。
若必须在 MSVC 下检查，只能用 `/fsanitize=address`（没有 UBSan），
把 `-DCMAKE_CXX_FLAGS="/fsanitize=address"` 传给 CMake 即可，此时不要使用
`-DENABLE_SANITIZERS=ON`（该选项只支持 GCC/Clang）。

---

## 4. 常见问题（FAQ）

**Q1. 构建一直报 `undefined reference to 'String::...'`？**
说明对应函数还没实现（或实现文件没有加进 `STRING_SOURCES`）。
这是起始框架的正常状态，逐个把 `include/my_string.h` 里的声明实现完即可。

**Q2. `ctest --test-dir` 报 `unrecognized option`？**
CMake 版本低于 3.20，改用 `cd build && ctest --output-on-failure`。

**Q3. 配置时报错说编译器不支持 `-fsanitize=address,undefined`？**
ASan/UBSan 默认开启，因此配置阶段会做真实探测，不支持就会直接报错。可以：
换用 `-DCMAKE_CXX_COMPILER=clang++`；升级 GCC；或改用 WSL2 / Docker。
如果只想开 ASan，按 3.2 节手动指定 `-fsanitize=address`（同时加
`-DENABLE_SANITIZERS=OFF`）；确实不需要 sanitizer 时加 `-DENABLE_SANITIZERS=OFF`
做普通构建（但不满足作业的内存检查要求）。

**Q4. ASan 报错里没有函数名和行号？**
确认构建类型是 `Debug`（默认即是，带 `-g`），并保留 `-fno-omit-frame-pointer`。
另外要保证报错位置和你的源码行号对得上——修改代码后记得重新编译。

**Q5. 被移动后的对象该怎么处理？**
只保证“可析构、可重新赋值”，不要读取它的内容或 `size()`。
随附测试就是这么用的。建议移动后把源对象的指针置空、长度容量清零，
析构时用 `delete[] nullptr` 兜底。

**Q6. 为什么头文件叫 `my_string.h` 而不是 `string.h`？**
`include/` 会通过 `-Iinclude` 进入头文件搜索路径。若把文件命名为 `string.h`，
在某些工具链（尤其 macOS）上，标准库内部的 `#include <string.h>` 会先找到你的
文件而不是 C 标准库头文件，导致编译失败。改名可以彻底避开这个冲突。

**Q7. 对编译警告有什么要求？**
不要求 `-Werror`，但验收标准是“无警告编译”。完成前请把 `-Wall -Wextra -Wpedantic`
下的警告清零（常见如未使用参数、有符号/无符号比较等）。

**Q8. 可以自己加测试文件吗？**
可以，也非常鼓励；但请**新建自己的文件**，不要修改 `tests/string_tests.cpp`。
随附测试是你验收的主要依据。

---

## 5. 完成前自检清单

- [ ] 默认（ASan + UBSan）构建：`cmake --build build -j` 无警告通过；
- [ ] `ctest --test-dir build --output-on-failure` 输出 `ALL TESTS PASSED`（412 项检查）；
- [ ] `-DENABLE_SANITIZERS=OFF` 的普通构建同样全部通过、无警告；
- [ ] sanitizer 构建下无 ASan/UBSan 报错；
- [ ] 空串、长串、多次扩容、自赋值、自移动、自插入、自交换、非法位置异常都想过一遍；
- [ ] 没有使用 `std::string` / `std::string_view` / 任何 STL 容器；
- [ ] 没有修改 `tests/` 与公开接口签名；
