# Assignment 2 · 自定义 String 类

本仓库是本次作业的**起始框架**：文档、工程结构与自动测试已经就绪，
实现部分（`include/my_string.h` 的私有成员 + `src/my_string.cpp`）留给你来完成。

本次作业**只需要实现 `String` 类**，没有其他类、也没有附加部分。目标是用
`char` 数组手写一个简化版的 `std::string`，练习：

- 动态内存管理（`new[]` / `delete[]`）与 RAII；
- Rule of Five：拷贝/移动构造、拷贝/移动赋值、析构；
- 运算符重载：`+`、`[]`、`<<`、`>>` 与 `const char*` 转换；
- 深拷贝值语义，以及自赋值、自移动、自插入、自交换等边界情况；
- 强异常安全（分配失败时原对象不被破坏）与内存安全（ASan/UBSan 零报告）。

## 仓库结构

```text
assignment2-string/
├── CMakeLists.txt              # 构建脚本（自带 -DENABLE_SANITIZERS=ON 选项）
├── README.md                   # 本文件：概览、快速开始与验收
├── TASKS.md                    # 作业要求（接口、语义、约束）
├── docs/
│   └── build-and-test.md       # 构建 / 测试 / ASan+UBSan 详解与 FAQ
├── include/
│   └── my_string.h             # String 的公开接口（需要你补私有数据成员）
├── src/
│   └── my_string.cpp           # 实现文件（目前为空，需要你填写）
└── tests/
    └── string_tests.cpp        # 随附自动测试（请勿修改）
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

> **刚开始构建失败是预期的**：`src/my_string.cpp` 目前为空，链接阶段会报
> `undefined reference to 'String::...'`。按 [`TASKS.md`](TASKS.md) 完成实现后，
> 构建与测试即可通过。测试程序共 412 项检查，全部通过时最后输出 `ALL TESTS PASSED`。

也可以直接运行测试程序（不经过 CTest），输出与退出码相同：

```bash
./build/string_tests
```

### 内存检查（ASan + UBSan）

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

`-DENABLE_SANITIZERS=ON` 会先探测编译器是否支持
`-fsanitize=address,undefined`，再为编译和链接加上
`-fno-omit-frame-pointer -fno-sanitize-recover=all`；不支持时 CMake 会直接报错
并提示替代做法，不会静默跳过。手动指定标志、各平台注意事项与报错解读见
[`docs/build-and-test.md`](docs/build-and-test.md)。

## 验收标准

1. `tests/string_tests.cpp` 在常规构建与 ASan/UBSan 构建下全部通过；
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
