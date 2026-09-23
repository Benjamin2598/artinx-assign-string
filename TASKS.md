# 作业 2：实现自定义 String 类

## 1. 作业概述

在本次作业中，你需要设计并实现一个基于 `char` 数组的 `String` 类。
**本次作业只需要实现 `String` 这一个类**，没有其他类或附加任务。

完成标准：

- 代码在 C++17 下无警告编译，通过随附的全部自动测试；
- 在 AddressSanitizer + UndefinedBehaviorSanitizer 下无任何错误报告；
- 除“被移动后”的对象外，所有对象始终保存以 `'\0'` 结尾的字符串，空串同样是有效状态；
- 不使用 `std::string` 及任何 STL 容器代替自己实现的功能。

选做（bonus）：第 6 节的流运算符 `<<` / `>>`。不实现不影响上述验收；
实现了可以额外运行 bonus 测试（见 5、6 节）。

仓库中 `include/my_string.h` 已给出必须实现的公开接口，`src/my_string.cpp` 是空的；
你的工作就是补全私有数据成员并实现全部成员函数与运算符。

## 2. 目录结构

```text
assignment2-string/
├── CMakeLists.txt              # 构建脚本（含 sanitizer 选项）
├── README.md                   # 快速开始、验收标准与自检
├── TASKS.md                    # 本文件：作业要求
├── docs/
│   ├── build-and-test.md       # 构建、测试、ASan/UBSan 详解与 FAQ
│   └── guide.md                # 教学指南：先修概念 + 实现骨架
├── include/
│   └── my_string.h             # 公开接口（需要你补私有成员）
├── src/
│   └── my_string.cpp           # 实现文件（留空，需要你填写）
└── tests/
    ├── string_tests.cpp        # 基线自动测试（必做，请勿修改）
    └── stream_tests.cpp        # 流操作 bonus 测试（选做，请勿修改）
```

## 3. 功能要求

### 3.1 构造、析构与赋值（Rule of Five）

```cpp
String();                                     // 空字符串，设置一个合理的初始容量（例如 16）
String(const char* str);                      // C 字符串构造；nullptr 视为空串，不允许 UB
String(const String& other);                  // 拷贝构造（深拷贝）
String(String&& other) noexcept;              // 移动构造（窃取缓冲区）
~String();                                    // 析构，释放全部动态内存
String& operator=(const String& other);       // 复制赋值
String& operator=(String&& other) noexcept;   // 移动赋值
```

- 拷贝构造与复制赋值必须**深拷贝**：两个对象不共享任何缓冲区；
- 移动构造与移动赋值必须为 `noexcept`，可以“窃取”资源，但被移动对象必须保持
  “有效但内容未指定”的状态（见 4.2）；
- `noexcept` 是函数签名的一部分：声明里写了，`.cpp` 的定义里必须原样写，
  否则编译报错（见 [`docs/guide.md`](docs/guide.md)）；
- 复制赋值必须自赋值安全（`s = s` 不得释放自己的缓冲区）。

### 3.2 拼接与下标

```cpp
String operator+(const String& other) const;  // 拼接，返回新对象，不修改操作数

char& operator[](std::size_t index) noexcept;             // 不做边界检查
const char& operator[](std::size_t index) const noexcept;

char& at(std::size_t index);                  // 带边界检查
const char& at(std::size_t index) const;      // 越界抛出 std::out_of_range
```

`operator[]` 与 `std::string` 一致，不做边界检查，测试不会传入越界位置；
带检查的访问请使用 `at()`。

#### 异常最小知识（本作业唯一需要主动抛异常的地方）

`at()` 与 `insert()` 越界时要抛 `std::out_of_range`：

```cpp
#include <stdexcept>  // std::out_of_range

throw std::out_of_range("String::at: index out of range");
```

随附测试用下面的方式验证（你只要保证确实抛出即可）：

```cpp
try {
    (void)s.at(999);
} catch (const std::out_of_range&) {
    // 捕获到即说明实现正确
}
```

`std::bad_alloc`（`new[]` 失败时自动抛出）不需要你写 `throw`，但实现要在这种
失败下保持对象不变（见 4.1）。完整的异常、异常安全、移动语义与 `noexcept` 讲解见
[`docs/guide.md`](docs/guide.md)。

### 3.3 长度与容量

```cpp
std::size_t size() const noexcept;      // 当前长度（不含结尾 '\0'）
std::size_t capacity() const noexcept;  // 当前容量（不含结尾 '\0'）
```

容量语义统一为：**`capacity()` 不包含结尾的 `'\0'`**，缓冲区实际可容纳
`capacity() + 1` 个字节，且恒有 `c_str()[size()] == '\0'`。
你的测试与文档描述都应与该语义一致（随附测试即按此语义编写）。

### 3.4 插入与追加

```cpp
void insert(std::size_t pos, const String& str);  // 在 pos 处插入 str
void push_back(char ch);                          // 末尾追加一个字符
```

- `insert`：`pos > size()` 时抛出 `std::out_of_range`；
- `insert` 必须正确处理自插入（如 `s.insert(0, s)`、`s.insert(s.size(), s)`），
  扩容与原地移动都不能破坏源数据；
- `push_back`：容量不足时自动扩容，追加后仍以 `'\0'` 结尾；
- 两个函数在内存分配失败时都必须保持原对象内容不变。

### 3.5 类型转换

```cpp
const char* c_str() const noexcept;           // 返回以 '\0' 结尾的内部缓冲区
operator const char*() const noexcept;        // 隐式转换
```

- `c_str()` 返回的指针仅在对象未被修改、未被移动、未析构前有效；
  对任何对象（包括被移动过的）调用 `c_str()` 都必须返回一个有效的、以 `'\0'`
  结尾的指针，不得返回 `nullptr` 或导致 UB。

> 流运算符 `<<` / `>>` 是**选做内容**，已移到第 6 节。

### 3.6 交换

```cpp
void swap(String& other) noexcept;  // 交换两个对象的全部内容，自交换也必须安全
```

交换后两个对象的长度、容量与数据整体互换，且都保持以 `'\0'` 结尾。

### 3.7 接口速查

| 成员 | 说明 |
| --- | --- |
| `String()` | 空串，容量合理（建议 ≥ 16） |
| `String(const char*)` | 由 C 字符串构造，`nullptr` 视为空串 |
| 拷贝 / 移动构造、拷贝 / 移动赋值 | Rule of Five，深拷贝 / 窃取 |
| `operator+` | 拼接，返回新对象 |
| `operator[]` | 无边界检查（与 `std::string` 一致） |
| `at()` | 有边界检查，越界抛 `std::out_of_range` |
| `size()` / `capacity()` | 长度 / 容量（容量不含 `'\0'`） |
| `insert(pos, str)` | 插入，`pos > size()` 抛异常，自插入安全 |
| `push_back(ch)` | 末尾追加一个字符，自动扩容 |
| `c_str()` / `operator const char*` | 转 C 字符串 |
| `swap(other)` | 交换全部内容，自交换安全 |
| `operator<<` / `operator>>` | 流输入输出（**选做**，见第 6 节） |

## 4. 设计约束

### 4.1 内存管理

- 每个对象独占自己的缓冲区，析构时释放干净，不允许内存泄漏或双重释放；
- 长度达到容量时应重新分配内存并复制原有数据，扩容后容量不得小于所需长度；
- 复制赋值与扩容必须**先成功分配新内存，再修改原对象**，至少提供强异常安全保证：
  一旦抛出异常（如 `std::bad_alloc`），原对象内容必须保持原样。
  这里的“强异常安全”指：操作要么成功，要么对象与操作前完全一样，不能出现
  “旧缓冲区已释放、新缓冲区又没分配成功”的悬空状态；实现口诀是
  “先分配成功、再释放旧的”，详见 [`docs/guide.md`](docs/guide.md)。

### 4.2 被移动后的对象

移动操作后，源对象处于“**有效但内容未指定**”的状态：

- 必须可以安全析构、可以重新赋值；
- 程序不得依赖其内容或 `size()` 的取值；
- 再次对其实施移动等操作也不允许出现 UB 或双重释放；
- 移动安全的具体做法（源对象置空、自移动特判等）见
  [`docs/guide.md`](docs/guide.md)。

### 4.3 自操作

自赋值、自移动赋值、自插入（`s.insert(pos, s)`）与自交换都必须有确定行为：
语义等价于“对同一对象做一次普通操作”，不得出现数据损坏或重复释放。

### 4.4 允许与禁止

- **禁止**：使用 `std::string`、`std::string_view` 或任何 STL 容器
  （`std::vector`、`std::list`、`std::map`……）来替代本作业要求你自己实现的功能；
- **允许**：`new[]` / `delete[]`、`std::move`、异常类型（如 `std::out_of_range`）、
  `<iostream>` / `<istream>` / `<ostream>` 等必要的标准库设施；
- **建议**：尽量不使用 `<cstring>` 的 `strlen` / `memcpy` / `memmove`，
  自己写循环完成对应功能——这是本作业的训练目标之一。使用它们不算违规，
  但你必须说得清其语义与和手写实现的关系；
- 不得修改 `tests/` 下的测试文件；不得改动 `include/my_string.h` 中公开接口的
  函数名、参数、返回类型、`const` / `noexcept` 与异常语义。

## 5. 测试与验收

仓库已自带自动测试（`tests/string_tests.cpp`，当前版本 **401 项检查**），
它是你自测和验收的主要依据：

```bash
# 默认构建（已开启 AddressSanitizer + UndefinedBehaviorSanitizer）+ 测试
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

# 如需不带 sanitizer 的普通构建
cmake -S . -B build-plain -DENABLE_SANITIZERS=OFF
cmake --build build-plain -j
ctest --test-dir build-plain --output-on-failure
```

sanitizer 默认开启：配置阶段会探测编译器是否支持，支持才继续，
不支持（或使用 MSVC）则直接报错，可用 `-DENABLE_SANITIZERS=OFF` 关闭
（但关闭后不满足本作业的内存检查要求）。

选做的流操作单独提供了一份测试（`tests/stream_tests.cpp`，11 项检查），
需要用选项开启才会构建与运行（默认关闭，不影响上面的基线测试）：

```bash
cmake -S . -B build -DENABLE_BONUS_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure   # 会同时运行基线与 bonus 测试
```

**不强制要求自行编写测试**，能通过随附测试、并理解其中的边界语义即可；
非常鼓励你在完成前补充自己的边界用例（例如放到自己的临时文件中，不要修改
`tests/string_tests.cpp`）。评分不会只看“测试是否变绿”，还会关注实现是否真正满足
上述语义与内存安全要求，请勿针对测试输出硬编码。

`docs/build-and-test.md` 中给出了更详细的构建说明、单测失败定位方法、
sanitizer 报告解读与常见问题排查；异常、异常安全、移动语义、`noexcept`
以及关键函数的实现骨架见 [`docs/guide.md`](docs/guide.md)。

## 6. Bonus（选做）：流运算符 `<<` / `>>`

本节是**选做内容**：不实现不影响第 1~5 节的验收；实现了可以额外运行
`tests/stream_tests.cpp`（开启 `-DENABLE_BONUS_TESTS=ON`）进行验证。
接口已在 `include/my_string.h` 中声明：

```cpp
friend std::ostream& operator<<(std::ostream& os, const String& str);
friend std::istream& operator>>(std::istream& is, String& str);
```

要求：

- `operator<<` 按 `size()` 写出内容（可含 `'\0'`，与 `std::string` 一致），
  并返回 `os`；
- `operator>>` 跳过前导空白，读到空白或流结束为止，并返回 `is`；
- `operator>>` 未读到任何字符时置 `failbit`，且 `str` **保持原值不变**；
- 推荐做法：先用 `std::istream::sentry`（或手动判断流状态）跳过空白，
  再读到临时 `String`，成功后再提交给 `str`（这样读取失败时原值不被破坏）。

实现思路与代码骨架见 [`docs/guide.md`](docs/guide.md) 的 Bonus 一节；
测试命令见 5 节。
