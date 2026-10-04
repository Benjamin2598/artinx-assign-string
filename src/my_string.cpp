// ============================================================================
// 作业 2：String 类的实现文件
//
// 目前本文件是空的：构建时链接阶段会报 "undefined reference to `String::...`"，
// 这是预期现象。请先到 include/my_string.h 中补好私有数据成员，再在这里实现
// 所有声明过的成员函数与运算符。
//
// 如果你想拆成多个 .cpp 文件，请同步修改根目录 CMakeLists.txt 中的
// STRING_SOURCES 列表。
//
// 实现清单（与 include/my_string.h 一一对应）：
//   [ ] String() / String(const char*) / 拷贝构造 / 移动构造 / 析构
//   [ ] 复制赋值 operator=(const String&) / 移动赋值 operator=(String&&)
//   [ ] operator+ / operator[]（含 const 版本）/ at（含 const 版本）
//   [ ] size / capacity
//   [ ] insert / push_back
//   [ ] c_str / operator const char*
//   [ ] swap
//   [ ] friend operator<< / operator>>
//
// 完成后按 docs/build-and-test.md 的步骤构建、运行测试并做 ASan/UBSan 检查。
// ============================================================================

#include "my_string.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
constexpr std::size_t kMinCapacity = 16;  // 容量契约：默认构造的空串容量 >= 16

std::size_t raw_length(const char* s) noexcept {
    std::size_t length = 0;
    while (s[length] != '\0') {
        ++length;
    }
    return length;
}

void raw_copy(char* dst, const char* src, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
}

// 与 memmove 语义相同：允许 src、dst 指向同一缓冲区的重叠区间
void raw_move(char* dst, const char* src, std::size_t count) noexcept {
    if (dst < src) {
        for (std::size_t i = 0; i < count; ++i) {
            dst[i] = src[i];
        }
    } else {
        for (std::size_t i = count; i > 0; --i) {
            dst[i - 1] = src[i - 1];
        }
    }
}

std::size_t growth_target(std::size_t current_capacity, std::size_t needed) noexcept {
    // 增长策略：max(所需长度, 当前容量 x 2, kMinCapacity)
    // 这样连续 push_back 的摊销复杂度是 O(1)
    const std::size_t max_capacity = std::numeric_limits<std::size_t>::max();
    const std::size_t doubled_capacity =
        current_capacity > max_capacity / 2 ? max_capacity : current_capacity * 2;
    const std::size_t target = needed > doubled_capacity ? needed : doubled_capacity;
    return target > kMinCapacity ? target : kMinCapacity;
}
}  // namespace

// TODO: 在此实现 include/my_string.h 中声明的所有成员函数与运算符。
