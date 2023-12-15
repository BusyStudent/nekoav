#include <gtest/gtest.h>
#include "../nekoav/detail/cxx20.hpp"
#include "../nekoav/libc.hpp"

std::string func1(int a) {
    auto l = NEKO_SOURCE_LOCATION();
    EXPECT_STREQ("func1", l.function_name());
    return NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
}

namespace namespace1 {
enum Enum1 {
    Element1,
};

std::string func2(Enum1 a) {
    auto l = NEKO_SOURCE_LOCATION();
    EXPECT_STREQ("func2", l.function_name());
    return NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
}

class A {
    bool b = false;
};

template <typename T>
class B {
public:
    std::string operator()(T a) {
        auto l = NEKO_SOURCE_LOCATION();
        EXPECT_STREQ("operator()", l.function_name());
        return NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
    }
};
template <typename T, T x>
std::string func4(B<T> b = B<T>()) {
    auto l = NEKO_SOURCE_LOCATION();
    EXPECT_STREQ("func4", l.function_name());
    return NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
}
}

std::string func3(namespace1::Enum1 a) {
    auto l = NEKO_SOURCE_LOCATION();
    EXPECT_STREQ("func3", l.function_name());
    return NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
}

bool operator==(int a, namespace1::A b) {
    auto l = NEKO_SOURCE_LOCATION();
    EXPECT_STREQ("operator==", l.function_name());
    NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
    return true;
}

TEST(CXXlibstd, source_location) {
    auto func = []() {
        auto l = NEKO_SOURCE_LOCATION();
        EXPECT_STREQ("operator()", l.function_name());
        return NekoAV::libc::asprintf("%s:%d %s", l.file_name(), l.line(), l.function_name());
    };

    std::cout << func() << std::endl;
    std::cout << func1(2) << std::endl;
    std::cout << namespace1::func2(namespace1::Element1) << std::endl;
    std::cout << func3(namespace1::Element1) << std::endl;
    namespace1::A a;
    a == true;
    std::cout << namespace1::func4<int, 2>() << std::endl;
    namespace1::B<int> b;
    std::cout << b(1) << std::endl;
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}