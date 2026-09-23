# 先修知识：异常、移动语义与 noexcept

本作业的接口里出现了 `throw std::out_of_range`、`noexcept`、`std::move` 等写法。
如果你还没有系统学过这些内容，读完这一篇就够用了——本文只讲本作业实际会用到的部分。

---

## 1. 异常最小知识

**异常（exception）** 是函数无法完成工作时"通知调用者"的机制：用 `throw` 抛出，
调用者用 `try` / `catch` 捕获。没人捕获时，异常会一路上抛，最终程序调用
`std::terminate()` 终止（通常看到 `terminate called after throwing an instance of ...`）。

```cpp
#include <stdexcept>   // std::out_of_range

void f() {
    throw std::out_of_range("index out of range");   // 抛出
}

int main() {
    try {
        f();                                  // 可能抛异常的调用
    } catch (const std::out_of_range& e) {    // 捕获（推荐按 const 引用捕获）
        // e.what() 是抛出时传入的错误信息
    }
}
```

要点：

- 捕获请写 `catch (const T& e)`，不要按值捕获（会丢失派生类型信息）；
- `catch (...)` 可以捕获所有异常，但本作业用不到；
- 异常类型都定义在标准库头文件里：`std::out_of_range` 在 `<stdexcept>`，
  `std::bad_alloc` 在 `<new>`；
- **不要用异常做正常流程控制**，只在"确实无法完成"时使用。

### 本作业涉及哪些异常

| 异常 | 谁抛出 | 什么时候 |
| --- | --- | --- |
| `std::out_of_range` | **你**（在 `at()` / `insert()` 中 `throw`） | 下标越界、`insert` 位置非法 |
| `std::bad_alloc` | `new[]` 自动抛出 | 内存不足时，你不需要写 `throw`，但实现要在这种失败下保持正确 |
| —— | —— | `operator[]` 越界是未定义行为（**不抛异常**）；`operator>>` 读取失败用流的 `failbit`，**也不抛异常** |

### 本作业唯一需要你主动写的 throw

`at()` 与 `insert()` 越界时抛 `std::out_of_range`：

```cpp
#include <stdexcept>

char& String::at(std::size_t index) {
    if (index >= size_) {
        throw std::out_of_range("String::at: index out of range");
    }
    return data_[index];
}
```

随附测试用下面的方式验证（你不需要写这段，只要保证确实抛出即可）：

```cpp
try {
    (void)s.at(999);
} catch (const std::out_of_range&) {
    // 捕获到就说明实现正确
}
```

---

## 2. 异常安全（exception safety）

"异常安全"讨论的是：**一个操作抛异常时，对象和程序会处于什么状态**。常见三个级别：

| 级别 | 含义 |
| --- | --- |
| no-throw（不抛保证） | 操作保证不抛异常 |
| strong（强保证） | 要么成功，要么对象与操作前**完全一样**（提交/回滚语义） |
| basic（基本保证） | 允许状态改变，但不能泄漏内存、不能破坏对象不变量 |

本作业要求相关操作达到**强异常安全**，具体到代码就是一句话：

> `new[]` 失败抛出 `std::bad_alloc` 时，原字符串的内容必须保持原样，
> 不允许出现"旧缓冲区已经释放、新缓冲区又没分配成功"的悬空状态。

### 实现口诀：先分配成功，再释放旧的

```cpp
// 扩容（ensure_capacity / insert 的扩容路径）
char* fresh = new char[target + 1];   // 1. 先分配：失败会抛出，此时对象还没被改动
/* 拷贝旧数据、写结尾 '\0' */
delete[] data_;                       // 2. 成功之后才释放旧缓冲区
data_ = fresh;
capacity_ = target;
```

反例（**会出问题**）：

```cpp
delete[] data_;                       // 先释放旧缓冲区
data_ = new char[target + 1];         // 如果这里抛出 bad_alloc……
                                      // data_ 指向已释放内存 → 悬空指针、double free
```

其它两条常用规则：

- **复制赋值用 copy-and-swap**：先拷贝构造一个临时对象（可能抛出，此时 `*this` 未变），
  成功后再 `swap`；
- **自赋值、自插入、自交换**：保证源数据在被覆盖之前不会被销毁。

> 随附测试不会模拟"内存不足"（很难稳定复现），所以强异常安全主要靠写法和代码审查；
> 但把上面两条规则写反，很容易造成真实的内存泄漏或悬空指针，**ASan 会直接报出来**。

---

## 3. 移动语义速览

### 为什么需要移动

拷贝是 O(n) 的深拷贝。而像 `String("hello")`、函数返回值这类**临时对象**马上就销毁了，
如果可以"偷走"它的缓冲区指针，就能把 O(n) 降到 O(1)。

```cpp
String a("hello");
String b(std::move(a));   // 移动构造：b 接管 a 的缓冲区
                          // 之后 a 处于"被移动后"状态，不要再依赖其内容
```

- `std::move(x)` 本身**不移动任何东西**，它只是把 `x` 转成右值引用，表示"允许被偷"；
  需要 `#include <utility>`；
- `String&&`（右值引用）只能绑定到临时对象或 `std::move` 的结果。

### 被移动后（moved-from）的对象

本作业的约定是"**有效但内容未指定**"：

- 可以安全析构、可以重新赋值（随附测试就是这么用的）；
- **不要**读它的内容或 `size()`；
- 析构函数要能处理"缓冲区指针为空"的情况（`delete[] nullptr` 是合法的空操作）；
- 源对象的指针要被置空（`nullptr`），否则它析构时会把你刚偷走的缓冲区释放掉。

### 自移动（`s = std::move(s)`）

移动赋值的第一步通常是"释放自己原来的缓冲区"。如果 `this == &other`，
就会先把自己的缓冲区删掉，再去偷……已经没有的东西。所以必须先判断：

```cpp
String& String::operator=(String&& other) noexcept {
    if (this == &other) {
        return *this;          // 自移动：直接返回
    }
    delete[] data_;            // 释放自己的旧缓冲区
    data_ = other.data_;       // 接管 other 的缓冲区
    /* 交换/复制 size_、capacity_ 并把 other 置空 */
    return *this;
}
```

---

## 4. `noexcept` 是什么

`noexcept` 写在函数声明后面，是**函数签名的一部分**（C++17 起更是函数类型的一部分）：

```cpp
String(String&& other) noexcept;              // 声明
String& operator=(String&& other) noexcept;
void swap(String& other) noexcept;
```

它表示"承诺这个函数不会让异常逃出去"。注意：

- 它**不是**"函数里不能出现 throw"，而是"如果异常真的逃出去，程序会直接调用
  `std::terminate()` 终止"——比不写更严格；
- 本作业里给 `noexcept` 的函数都是不会分配内存、不会抛异常的操作：
  移动构造 / 移动赋值、`operator[]`、`size()` / `capacity()`、`c_str()`、
  `const char*` 转换、`swap()`；
- `at()` / `insert()` **不是** `noexcept`，因为它们需要抛 `std::out_of_range`；
- 为什么移动操作要 `noexcept`：标准库容器在扩容时据此决定"移动还是拷贝"
  （`std::move_if_noexcept`）。本作业的移动只是窃取指针，天然不会抛。

### 新手最容易踩的坑：定义里漏写 `noexcept`

如果声明里有 `noexcept`，`.cpp` 里的定义必须**原样保留**，否则编译报错：

```cpp
// 声明（include/my_string.h）
String(String&& other) noexcept;

// 定义（src/my_string.cpp）—— 必须也写 noexcept
String::String(String&& other) noexcept { /* ... */ }
```

漏写时的报错（GCC）：

```text
error: declaration of 'String::String(String&&)' has a different exception specifier
note: from previous declaration 'String(String&&) noexcept'
```

看到这句话，补上 `noexcept` 即可。其它带 `noexcept` 的函数（`operator[]`、
`size`、`capacity`、`c_str`、转换、`swap`）同理，定义都要和声明保持一致。

---

## 5. 常见困惑

**Q：`at()` 和 `operator[]` 有什么区别？**
`operator[]` 与 `std::string` 一致，不做检查，越界是未定义行为；`at()` 做检查，
越界抛 `std::out_of_range`。这就是为什么 `at()` 不能是 `noexcept`。

**Q：我能在 `noexcept` 函数里 `throw` 吗？**
能编译，但异常逃出去程序会立即 `terminate`，等于制造崩溃。移动构造 / 移动赋值里
不要做会分配内存的事（本作业的移动只是窃取指针，所以没问题）。

**Q：被移动后的对象能调用 `c_str()` 吗？**
按本作业的接口约定要返回一个有效的 C 字符串；但它的**内容未指定**，
不要拿它去比较或计算长度。

**Q：为什么 `insert(pos > size())` 要抛异常而不是返回错误码？**
因为接口与 `std::string` 保持一致；异常能保证"要么成功、要么什么都没发生"，
调用者不容易忽略错误。这是本作业唯一需要你主动 `throw` 的地方。

**Q：如果我不小心先 `delete[]` 再 `new`，但测试全过了，算对吗？**
不算。测试不会制造 `bad_alloc`，所以这种做法在测试里可能看不出来，但它违反了
强异常安全要求；写 ASan 检查 + 代码审查时会被发现。
