# Assignment 2 · 自定义 String 类

本仓库是本次作业的**起始框架**：文档、工程结构与自动测试已经就绪，
实现部分（`include/my_string.h` 的私有成员 + `src/my_string.cpp`）留给你来完成。

本次作业**只需要实现 `String` 类**，没有其他类、也没有附加部分。目标是用
`char` 数组手写一个简化版的 `std::string`，练习：

- 动态内存管理（`new[]` / `delete[]`）与 RAII；
- Rule of Five：拷贝/移动构造、拷贝/移动赋值、析构；
- 运算符重载：`+`、`[]` 与 `const char*` 转换（流运算符 `<<` / `>>` 为选做 bonus）；
- 深拷贝值语义，以及自赋值、自移动、自插入、自交换等边界情况；
- 强异常安全（分配失败时原对象不被破坏）与内存安全（ASan/UBSan 零报告）。

> 没学过异常或 `noexcept` 也不影响开始：本作业只用到很少一点。
> 建议先花 10 分钟浏览 [`docs/guide.md`](docs/guide.md)
> （先修概念 + 关键函数的实现骨架，含新手常见报错）。

## 仓库结构

```text
assignment2-string/
├── CMakeLists.txt              # 构建脚本（默认开启 ASan+UBSan，可关闭）
├── README.md                   # 本文件：概览、快速开始与验收
├── TASKS.md                    # 作业要求（接口、语义、约束）
├── docs/
│   ├── build-and-test.md       # 构建 / 测试 / ASan+UBSan 详解与 FAQ
│   └── guide.md                # 教学指南：先修概念 + 实现骨架
├── include/
│   └── my_string.h             # String 的公开接口（需要你补私有数据成员）
├── src/
│   └── my_string.cpp           # 实现文件（目前为空，需要你填写）
└── tests/
    ├── string_tests.cpp        # 基线自动测试（必做，请勿修改）
    └── stream_tests.cpp        # 流运算符 bonus 测试（选做，请勿修改）
```

## 环境要求

| 项目 | 要求 |
| --- | --- |
| 操作系统 | Linux 或 macOS（Windows 建议使用 WSL2） |
| 编译器 | 支持 C++17 的 GCC / Clang |
| 构建工具 | CMake ≥ 3.14 |
| 内存检查 | AddressSanitizer + UndefinedBehaviorSanitizer（GCC/Clang 自带，无需安装） |

## 快速开始

在仓库根目录执行：

```bash
# 1. 配置 + 构建
cmake -S . -B build
cmake --build build -j

# 2. 运行全部自动测试
ctest --test-dir build --output-on-failure
```

> **默认构建就开启了 ASan + UBSan**（配置阶段会探测编译器支持）。
> 如果想做一次不带 sanitizer 的普通构建，加 `-DENABLE_SANITIZERS=OFF`：
>
> ```bash
> cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
> cmake --build build-plain -j
> ctest --test-dir build-plain --output-on-failure
> ```
>
> **刚开始构建失败是预期的**：`src/my_string.cpp` 目前为空，链接阶段会报
> `undefined reference to 'String::...'`。按 [`TASKS.md`](TASKS.md) 完成实现后，
> 构建与测试即可通过。基线测试共 401 项检查，全部通过时最后输出 `ALL TESTS PASSED`。

也可以直接运行测试程序（不经过 CTest），输出与退出码相同：

```bash
./build/string_tests
```

### 选做：流运算符测试（bonus）

流运算符 `<<` / `>>` 是选做内容，测试单独放在 `tests/stream_tests.cpp`，
默认不构建（所以不做 bonus 也能全绿）：

```bash
cmake -S . -B build -DENABLE_BONUS_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure   # 基线与 bonus 一起运行（401 + 11 项）
```

### 内存检查（ASan + UBSan）

ASan + UBSan 已默认开启，所以上文 `build/` 里的测试就是内存检查版本：
配置阶段会先做一次真实的编译 + 链接探测，支持 `-fsanitize=address,undefined`
才继续；不支持（或使用 MSVC）时 CMake 直接报错，并提示改用 GCC/Clang 或加
`-DENABLE_SANITIZERS=OFF` 关闭——不会静默退化成普通构建。

显式写出开关（与默认行为等价）或需要独立目录时：

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

开启后编译与链接都会加上
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`。
手动指定标志、各平台注意事项、常见报错解读见
[`docs/build-and-test.md`](docs/build-and-test.md)。

## 验收标准

1. `tests/string_tests.cpp`（401 项）在默认的 ASan/UBSan 构建与
   `-DENABLE_SANITIZERS=OFF` 的普通构建下均全部通过；
   （选做）开启 `-DENABLE_BONUS_TESTS=ON` 后，`tests/stream_tests.cpp`（11 项）
   也全部通过；
2. 无编译警告（工程统一开启 `-Wall -Wextra -Wpedantic`）；
3. 未使用 `std::string`、`std::string_view` 或任何 STL 容器；
4. 边界情况正确：空串、长串、多次扩容、自赋值、自移动、自插入、自交换，
   非法 `insert` / `at` 位置抛出 `std::out_of_range`；
5. 不得修改 `tests/` 与公开接口签名（`include/my_string.h` 中的函数名、参数、
   返回类型、`const` / `noexcept` 均不可改动；私有成员可自由添加）；
6. 代码可读，命名与注释清晰；评分可能抽查实现细节与边界情况，请勿针对测试硬编码。

## 遇到问题？

先看 [`docs/build-and-test.md`](docs/build-and-test.md) 的 FAQ。仍然无法解决时再联系助教，
并在提问时附上完整命令、完整报错信息以及操作系统与编译器版本。
