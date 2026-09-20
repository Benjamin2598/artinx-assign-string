// String 类的自动测试。
//
// 说明：
//   * 测试不依赖交互输入，失败时以非零退出码结束（供 CTest 使用）；
//   * 轻量断言宏在 -DNDEBUG 下依然生效；
//   * 测试同样不使用 std::string 及其运算，<sstream> 仅用于验证 << / >>；
//   * 不规定初始容量的具体数值（只要求"合理"），断言仅依赖与数值无关的语义。

#include "my_string.h"

#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: " << expr << '\n';
    }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

bool equals_chars(const char* actual, const char* expected) {
    std::size_t i = 0;
    for (; expected[i] != '\0'; ++i) {
        if (actual[i] != expected[i]) {
            return false;
        }
    }
    return actual[i] == '\0';
}

bool equals_cstr(const String& value, const char* expected) {
    return equals_chars(value.c_str(), expected);
}

void check_cstr(const String& value, const char* expected, const char* expr,
                const char* file, int line) {
    ++g_checks;
    if (!equals_cstr(value, expected)) {
        ++g_failures;
        std::cerr << file << ':' << line << ": CHECK failed: " << expr
                  << " (actual=\"" << value.c_str() << "\", expected=\"" << expected
                  << "\")\n";
    }
}

#define CHECK_CSTR(value, expected)                                                   \
    check_cstr((value), (expected), "c_str(" #value ") == expected", __FILE__,        \
               __LINE__)

#define CHECK_OUT_OF_RANGE(expr)                                                      \
    do {                                                                              \
        bool caught = false;                                                          \
        try {                                                                         \
            (void)(expr);                                                             \
        } catch (const std::out_of_range&) {                                          \
            caught = true;                                                            \
        } catch (...) {                                                               \
        }                                                                             \
        check(caught, #expr " throws std::out_of_range", __FILE__, __LINE__);         \
    } while (false)

// 结构不变量：size() <= capacity()，且 c_str()[size()] 恒为 '\0'
#define CHECK_INVARIANTS(value)                                                       \
    do {                                                                              \
        check((value).size() <= (value).capacity(),                                   \
              "size() <= capacity() (" #value ")", __FILE__, __LINE__);               \
        check((value).c_str()[(value).size()] == '\0',                                \
              "c_str()[size()] == '\\0' (" #value ")", __FILE__, __LINE__);           \
    } while (false)

// 生成由 count 个相同字符组成的字符串（不使用 std::string）
String repeat(char ch, std::size_t count) {
    String result;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back(ch);
    }
    return result;
}

void run(const char* name, void (*test)()) {
    const int before = g_failures;
    std::cout << "[ RUN      ] " << name << '\n';
    test();
    std::cout << (g_failures == before ? "[       OK ] " : "[   FAILED ] ") << name << '\n';
}

// ---------------------------------------------------------------------------
// 构造、析构与空字符串不变量
// ---------------------------------------------------------------------------

void test_default_constructed_empty() {
    String s;
    CHECK(s.size() == 0u);
    CHECK(s.capacity() >= s.size());  // 只要求容量不小于长度，不规定初始容量的具体值
    CHECK(s.c_str() != nullptr);
    CHECK(s.c_str()[0] == '\0');      // 空串仍是以 '\0' 结尾的有效字符串
    CHECK_CSTR(s, "");
}

void test_construct_from_c_str() {
    String s("hello");
    CHECK(s.size() == 5u);
    CHECK(s.capacity() >= 5u);
    CHECK_CSTR(s, "hello");
    CHECK(s[0] == 'h');
    CHECK(s[4] == 'o');

    String empty("");
    CHECK(empty.size() == 0u);
    CHECK(empty.c_str()[0] == '\0');
    CHECK_CSTR(empty, "");

    String from_null(nullptr);        // 作业要求：nullptr 视为空串
    CHECK(from_null.size() == 0u);
    CHECK_CSTR(from_null, "");

    const char* long_literal =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    String big(long_literal);
    CHECK(big.size() == 62u);
    CHECK_CSTR(big, long_literal);
    CHECK(big.c_str()[62] == '\0');
}

// ---------------------------------------------------------------------------
// 深拷贝、复制赋值与自赋值
// ---------------------------------------------------------------------------

void test_copy_semantics() {
    String a("hello");
    String b(a);                      // 拷贝构造
    CHECK(b.size() == a.size());
    CHECK_CSTR(b, "hello");
    CHECK(b.c_str() != a.c_str());    // 深拷贝：缓冲区相互独立

    b[0] = 'H';
    CHECK_CSTR(b, "Hello");
    CHECK_CSTR(a, "hello");           // 修改副本不影响原对象

    String c;
    c = a;                            // 复制赋值
    CHECK_CSTR(c, "hello");
    CHECK(c.c_str() != a.c_str());
    c[4] = '!';
    CHECK_CSTR(c, "hell!");
    CHECK_CSTR(a, "hello");

    String d("some much longer content that forces its own allocation");
    d = a;                            // 覆盖旧内容
    CHECK_CSTR(d, "hello");

    String e("nonempty");
    e = String();
    CHECK(e.size() == 0u);
    CHECK_CSTR(e, "");
}

void test_self_copy_assignment() {
    String s("hello");
    String& ref = s;                  // 经引用赋值，避免编译器对 x = x 的告警
    s = ref;
    CHECK_CSTR(s, "hello");
    CHECK(s.size() == 5u);
}

// ---------------------------------------------------------------------------
// 移动语义与接口类型
// ---------------------------------------------------------------------------

void test_move_semantics() {
    const char* text = "a fairly long string used to exercise move construction";
    String a(text);
    String b(std::move(a));           // 移动构造
    CHECK_CSTR(b, text);

    a = String("reused");             // 被移动后仍可重新赋值
    CHECK_CSTR(a, "reused");

    String c;
    c = std::move(b);                 // 移动赋值
    CHECK_CSTR(c, text);

    b = String("again");              // 被移动后仍可重新赋值
    CHECK_CSTR(b, "again");
}

void test_self_move_assignment() {
    String s("self move");
    String& ref = s;
    s = std::move(ref);               // 自移动赋值：内容未指定，但对象必须仍然有效
    s = String("after self move");    // 只检查：可以重新赋值（以及离开作用域时可析构）
    CHECK_CSTR(s, "after self move");
}

void test_type_properties() {
    static_assert(std::is_copy_constructible<String>::value, "必须支持拷贝构造");
    static_assert(std::is_copy_assignable<String>::value, "必须支持拷贝赋值");
    static_assert(std::is_move_constructible<String>::value, "必须支持移动构造");
    static_assert(std::is_move_assignable<String>::value, "必须支持移动赋值");
    static_assert(std::is_destructible<String>::value, "必须可析构");
    static_assert(std::is_same<decltype(std::declval<const String&>()[0]),
                               const char&>::value,
                  "const String 的 operator[] 必须返回 const char&");
    static_assert(std::is_same<decltype(std::declval<String&>()[0]), char&>::value,
                  "非 const String 的 operator[] 必须返回 char&");
}

// ---------------------------------------------------------------------------
// 拼接与下标
// ---------------------------------------------------------------------------

void test_operator_plus() {
    String a("hello");
    String b(" world");
    String c = a + b;
    CHECK_CSTR(c, "hello world");
    CHECK_CSTR(a, "hello");           // 原对象不受影响
    CHECK_CSTR(b, " world");

    String empty;
    CHECK_CSTR(empty + a, "hello");
    CHECK_CSTR(a + empty, "hello");
    CHECK((empty + empty).size() == 0u);
    CHECK_CSTR(a + a, "hellohello");  // 自身拼接

    String mixed = String() + a + String() + b;  // 空串参与链式拼接
    CHECK_CSTR(mixed, "hello world");

    // 连续拼接触发多次扩容，逐字符校验 "ab" x 100
    String chain;
    for (int i = 0; i < 100; ++i) {
        chain = chain + String("ab");
    }
    CHECK(chain.size() == 200u);
    CHECK(chain.capacity() >= chain.size());
    bool chain_ok = true;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        chain_ok = chain_ok && (chain[i] == (i % 2u == 0u ? 'a' : 'b'));
    }
    CHECK(chain_ok);
    CHECK_INVARIANTS(chain);

    String big = repeat('a', 500) + repeat('b', 500);
    CHECK(big.size() == 1000u);
    CHECK(big[0] == 'a');
    CHECK(big[499] == 'a');
    CHECK(big[500] == 'b');
    CHECK(big[999] == 'b');
}

void test_subscript() {
    String s("abc");
    CHECK(s[0] == 'a');
    s[1] = 'B';
    CHECK_CSTR(s, "aBc");
    CHECK(s[3] == '\0');              // s[size()] 可读，与 std::string 一致

    const String cs("const");
    CHECK(cs[0] == 'c');
    CHECK(cs[4] == 't');
    CHECK(cs[5] == '\0');             // const 对象也保持以 '\0' 结尾

    const char* converted = cs;       // 隐式转换为 const char*
    CHECK(converted == cs.c_str());
    CHECK(equals_chars(converted, "const"));
}

void test_at() {
    String s("abc");
    CHECK(s.at(0) == 'a');
    s.at(1) = 'B';
    CHECK_CSTR(s, "aBc");

    const String cs("xy");
    CHECK(cs.at(1) == 'y');
    CHECK_OUT_OF_RANGE(cs.at(2));
    CHECK_OUT_OF_RANGE(s.at(3));
    CHECK_OUT_OF_RANGE(String().at(0));
}

// ---------------------------------------------------------------------------
// 长度与容量
// ---------------------------------------------------------------------------

void test_size_and_capacity_semantics() {
    String s;
    CHECK(s.capacity() >= s.size());  // 初始容量不规定具体值
    if (s.capacity() == 0u) {
        s.push_back('a');             // 兼容"懒分配"的实现，此后 capacity >= 1
    }
    const std::size_t cap = s.capacity();

    // 填满到 capacity：capacity() 不含结尾 '\0'，因此不应触发扩容
    while (s.size() < cap) {
        s.push_back('a');
    }
    CHECK(s.size() == cap);
    CHECK(s.capacity() == cap);

    // 超出容量必须扩容，且内容完好、仍以 '\0' 结尾
    s.push_back('b');
    CHECK(s.size() == cap + 1);
    CHECK(s.capacity() >= s.size());
    CHECK(s[0] == 'a');
    CHECK(s[cap] == 'b');
    CHECK(s.c_str()[cap + 1] == '\0');
}

// ---------------------------------------------------------------------------
// 插入
// ---------------------------------------------------------------------------

void test_insert_basic() {
    String s("abcdef");
    s.insert(3, String("XY"));
    CHECK_CSTR(s, "abcXYdef");

    String t("world");
    t.insert(0, String("hello "));
    CHECK_CSTR(t, "hello world");

    String u("hello");
    u.insert(u.size(), String("!"));  // pos == size() 合法（追加）
    CHECK_CSTR(u, "hello!");

    String v("same");
    v.insert(2, String());            // 插入空串不改变内容
    CHECK_CSTR(v, "same");

    String w;                         // 向空串插入
    w.insert(0, String("first"));
    CHECK_CSTR(w, "first");

    // 插入触发扩容
    String big("z");
    big.insert(1, repeat('a', 100));
    CHECK(big.size() == 101u);
    CHECK(big[0] == 'z');
    CHECK(big[1] == 'a');
    CHECK(big[100] == 'a');
    CHECK(big.c_str()[101] == '\0');
}

void test_insert_self() {
    String s("abc");
    s.insert(0, s);                   // s.insert(0, s) 自插入
    CHECK_CSTR(s, "abcabc");

    String t("abcdef");
    t.insert(2, t);
    CHECK_CSTR(t, "ababcdefcdef");

    String u("xy");
    u.insert(u.size(), u);
    CHECK_CSTR(u, "xyxy");

    // 反复自插入：指数增长 + 多次扩容；对全部 512 个字符校验（奇偶位置交替 a/b）
    String v("ab");
    for (int i = 0; i < 8; ++i) {
        v.insert(v.size(), v);
    }
    CHECK(v.size() == 512u);
    CHECK(v.capacity() >= v.size());
    bool pattern_ok = true;
    for (std::size_t i = 0; i < v.size(); ++i) {
        pattern_ok = pattern_ok && (v[i] == (i % 2u == 0u ? 'a' : 'b'));
    }
    CHECK(pattern_ok);
    CHECK_INVARIANTS(v);
}

void test_insert_out_of_range() {
    String s("abc");
    CHECK_OUT_OF_RANGE(s.insert(4, String("x")));
    CHECK_CSTR(s, "abc");             // 失败时原对象内容不被破坏
    CHECK(s.size() == 3u);

    s.insert(3, String("d"));         // pos == size() 合法
    CHECK_CSTR(s, "abcd");

    String empty;
    CHECK_OUT_OF_RANGE(empty.insert(1, String("x")));
    CHECK_CSTR(empty, "");
}

void test_insert_into_long_string() {
    String s = repeat('a', 1000);
    String chunk("0123456789");
    s.insert(500, chunk);
    CHECK(s.size() == 1010u);
    CHECK_INVARIANTS(s);

    // 全内容校验：仅 [500, 510) 是数字，其余都是 'a'
    bool content_ok = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char expected =
            (i >= 500u && i < 510u) ? static_cast<char>('0' + (i - 500u)) : 'a';
        content_ok = content_ok && (s[i] == expected);
    }
    CHECK(content_ok);

    // 长串的深拷贝仍然正确且独立
    String copy(s);
    CHECK(copy.size() == s.size());
    CHECK(copy.c_str() != s.c_str());
    bool copy_ok = true;
    for (std::size_t i = 0; i < copy.size(); ++i) {
        copy_ok = copy_ok && (copy[i] == s[i]);
    }
    CHECK(copy_ok);
}

void test_insert_position_matrix() {
    // 在 "abcde"（长度 5）的每个合法位置插入 "XYZ"
    static const char* const expected[6] = {
        "XYZabcde", "aXYZbcde", "abXYZcde", "abcXYZde", "abcdXYZe", "abcdeXYZ",
    };
    for (std::size_t pos = 0; pos < 6u; ++pos) {
        String s("abcde");
        s.insert(pos, String("XYZ"));
        CHECK(s.size() == 8u);
        CHECK_CSTR(s, expected[pos]);
        CHECK_INVARIANTS(s);
    }
}

void test_insert_capacity_boundaries() {
    // 从"空串"出发，利用初始容量构造两个边界（不依赖具体容量数值）
    String s;
    if (s.capacity() == 0u) {
        s.push_back('0');  // 兼容"懒分配"的实现，此后 capacity >= 1
    }
    const std::size_t cap = s.capacity();
    const std::size_t base = s.size();
    CHECK(cap >= base);

    // 情况 1：插入后长度恰好等于 capacity（不应重新分配，走原地路径）
    const std::size_t add = cap - base;
    s.insert(base, repeat('x', add));  // 恰好填满到 capacity
    CHECK(s.size() == cap);
    CHECK(s.capacity() == cap);        // 未超过容量：不得重新分配
    if (base > 0u) {
        CHECK(s[0] == '0');
    }
    if (add > 0u) {
        CHECK(s[cap - 1u] == 'x');
    }
    CHECK_INVARIANTS(s);

    // 情况 2：再插入 1 个字符使长度恰为 capacity + 1（必须扩容）
    const std::size_t pos = cap / 2u;
    s.insert(pos, String("y"));
    CHECK(s.size() == cap + 1u);
    CHECK(s.capacity() >= s.size());
    CHECK(s[pos] == 'y');
    if (add > 0u) {
        CHECK(s[cap] == 'x');  // 原最后一个字符后移一位
    }
    CHECK_INVARIANTS(s);
}

void test_insert_self_with_growth() {
    // 自插入 + 中间位置 + 需要扩容：重叠复制最容易写错的组合
    String s;
    if (s.capacity() == 0u) {
        s.push_back('a');  // 兼容"懒分配"的实现
    }
    const std::size_t n = s.capacity();  // 先把长度填到恰好等于容量
    while (s.size() < n) {
        s.push_back(static_cast<char>('a' + static_cast<int>(s.size() % 26u)));
    }
    CHECK(s.size() == n);

    const std::size_t pos = 5;
    s.insert(pos, s);  // 长度翻倍，必然触发扩容
    CHECK(s.size() == 2u * n);
    CHECK(s.capacity() >= s.size());
    CHECK_INVARIANTS(s);

    // 全内容校验：前缀 [0, pos)、被插入的原串、以及原串从 pos 起的尾部
    bool content_ok = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::size_t src = 0;
        if (i < pos) {
            src = i;
        } else if (i < pos + n) {
            src = i - pos;
        } else {
            src = i - n;
        }
        const char expected = static_cast<char>('a' + static_cast<int>(src % 26u));
        content_ok = content_ok && (s[i] == expected);
    }
    CHECK(content_ok);
}

// ---------------------------------------------------------------------------
// swap 与容量契约
// ---------------------------------------------------------------------------

void test_swap() {
    String a("abcd");
    String b("hello, world");
    const std::size_t a_size = a.size();
    const std::size_t b_size = b.size();

    a.swap(b);
    CHECK(a.size() == b_size);
    CHECK(b.size() == a_size);
    CHECK_CSTR(a, "hello, world");
    CHECK_CSTR(b, "abcd");
    CHECK_INVARIANTS(a);
    CHECK_INVARIANTS(b);

    // 自交换
    String& ref = a;
    a.swap(ref);
    CHECK_CSTR(a, "hello, world");

    // 与空串交换
    String empty;
    empty.swap(b);
    CHECK(empty.size() == 4u);
    CHECK_CSTR(empty, "abcd");
    CHECK(b.size() == 0u);
    CHECK_CSTR(b, "");

    // 交换后两边都可继续使用
    a[0] = 'H';
    CHECK_CSTR(a, "Hello, world");
    b = String("reuse");
    CHECK_CSTR(b, "reuse");
}

void test_capacity_growth_contract() {
    // 连续追加：容量单调不减，且始终 >= size()
    String s;
    std::size_t prev_cap = s.capacity();

    for (std::size_t i = 0; i < 100u; ++i) {
        s.push_back(static_cast<char>('a' + static_cast<int>(i % 26u)));
        CHECK(s.capacity() >= prev_cap);
        CHECK(s.capacity() >= s.size());
        prev_cap = s.capacity();
    }
    CHECK(s.size() == 100u);
    CHECK_INVARIANTS(s);
}

// ---------------------------------------------------------------------------
// 空串与深拷贝独立性
// ---------------------------------------------------------------------------

void test_empty_string_operations() {
    String e;
    CHECK_INVARIANTS(e);

    String copy(e);
    CHECK(copy.size() == 0u);
    String assigned("nonempty");
    assigned = e;
    CHECK(assigned.size() == 0u);

    e.insert(0, String());  // 空串插入空串
    CHECK(e.size() == 0u);
    e.insert(0, e);         // 空串自插入
    CHECK(e.size() == 0u);
    e.insert(e.size(), e);
    CHECK(e.size() == 0u);
    CHECK_INVARIANTS(e);

    String sum = e + e;
    CHECK(sum.size() == 0u);
    CHECK_INVARIANTS(sum);

    String& ref = e;  // 自赋值 / 自移动（经引用避免编译器告警）
    e = ref;
    CHECK(e.size() == 0u);
    e = std::move(ref);
    e = String("recovered");
    CHECK_CSTR(e, "recovered");

    // 空串可以正常长起来（用一个新的空串，避免受前面内容影响）
    String fresh;
    fresh.push_back('x');
    CHECK_CSTR(fresh, "x");
    fresh[0] = 'y';
    CHECK_CSTR(fresh, "y");
    CHECK_INVARIANTS(fresh);
}

void test_deep_copy_independence_stress() {
    // 构造 200 字符的周期串 "0123456789" x 20
    String a;
    const String chunk("0123456789");
    for (int i = 0; i < 20; ++i) {
        a.insert(a.size(), chunk);
    }
    CHECK(a.size() == 200u);

    String b(a);  // 拷贝构造
    String c;
    c = a;        // 复制赋值

    // 对 b 做大量修改（自插入、扩容、改字符）
    b.insert(100, b);
    b.push_back('!');
    b[0] = '#';
    CHECK(b.size() == 401u);
    CHECK(b[0] == '#');
    CHECK(b[1] == '1');
    CHECK(b[99] == '9');
    CHECK(b[100] == '0');
    CHECK(b[399] == '9');
    CHECK(b[400] == '!');
    CHECK_INVARIANTS(b);

    // c 被覆盖为短串
    c = String("short");
    CHECK_CSTR(c, "short");

    // a 必须完全不受影响（逐字符校验）
    CHECK(a.size() == 200u);
    bool a_ok = true;
    for (std::size_t i = 0; i < a.size(); ++i) {
        a_ok = a_ok && (a[i] == static_cast<char>('0' + static_cast<int>(i % 10u)));
    }
    CHECK(a_ok);
    CHECK_INVARIANTS(a);
}

// ---------------------------------------------------------------------------
// 流操作与类型转换
// ---------------------------------------------------------------------------

void test_stream_output() {
    std::ostringstream os;
    os << String("hello") << ',' << String() << ',' << String("world");
    CHECK(equals_chars(os.str().c_str(), "hello,,world"));

    std::ostringstream os2;
    const String s("const output");
    os2 << s;
    CHECK(equals_chars(os2.str().c_str(), "const output"));
}

void test_stream_input() {
    std::istringstream input("hello world");
    String a;
    String b;
    input >> a >> b;
    CHECK_CSTR(a, "hello");
    CHECK_CSTR(b, "world");

    // 连续读取会替换旧内容
    std::istringstream input2("first second");
    String x("old content");
    input2 >> x;
    CHECK_CSTR(x, "first");
    input2 >> x;
    CHECK_CSTR(x, "second");

    // 前导空白被跳过
    std::istringstream input3("  \t\n leading");
    String y;
    input3 >> y;
    CHECK_CSTR(y, "leading");

    // 长单词（超过初始容量，触发多次扩容）
    const char* long_word =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
    std::istringstream input4(long_word);
    String z;
    input4 >> z;
    CHECK(z.size() > 64u);
    CHECK_CSTR(z, long_word);

    // 空输入：读取失败，原值保持不变
    std::istringstream empty("");
    String keep("keep");
    empty >> keep;
    CHECK(empty.fail());
    CHECK_CSTR(keep, "keep");
}

}  // namespace

int main() {
    run("default_constructed_empty", test_default_constructed_empty);
    run("construct_from_c_str", test_construct_from_c_str);
    run("copy_semantics", test_copy_semantics);
    run("self_copy_assignment", test_self_copy_assignment);
    run("move_semantics", test_move_semantics);
    run("self_move_assignment", test_self_move_assignment);
    run("type_properties", test_type_properties);
    run("operator_plus", test_operator_plus);
    run("subscript", test_subscript);
    run("at", test_at);
    run("size_and_capacity_semantics", test_size_and_capacity_semantics);
    run("insert_basic", test_insert_basic);
    run("insert_self", test_insert_self);
    run("insert_out_of_range", test_insert_out_of_range);
    run("insert_into_long_string", test_insert_into_long_string);
    run("insert_position_matrix", test_insert_position_matrix);
    run("insert_capacity_boundaries", test_insert_capacity_boundaries);
    run("insert_self_with_growth", test_insert_self_with_growth);
    run("swap", test_swap);
    run("capacity_growth_contract", test_capacity_growth_contract);
    run("empty_string_operations", test_empty_string_operations);
    run("deep_copy_independence_stress", test_deep_copy_independence_stress);
    run("stream_output", test_stream_output);
    run("stream_input", test_stream_input);

    std::cout << '\n' << "checks: " << g_checks << ", failures: " << g_failures << '\n';
    if (g_failures != 0) {
        std::cerr << "TESTS FAILED\n";
        return 1;
    }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
