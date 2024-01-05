#include "../nekoav/backtrace.hpp"
#include "../nekoav/threading.hpp"
#include "../nekoav/flags.hpp"
#include "../nekoav/time.hpp"
#include "../nekoav/log.hpp"
#include <map>

using namespace std::chrono_literals;

enum MyEnum {
    A, B, C, D, E, F, MyValues
};


enum class TestFlag : uint32_t {
    A = 1 << 0,
    B = 1 << 1
};
NEKO_DECLARE_FLAGS(TestFlag);

int add(int a, int b) {
    return a + b;
}
std::vector<int> returnVector(int a, int b, int c, int d, int e, int f) {
    return std::vector {a, b, c, d, e, f};
}
int main() {
    NekoAV::Thread worker;
    worker.postTask([]() {
        NEKO_DEBUG("A from worker");
        NekoAV::Backtrace();
    });
    worker.postTask([]() {
        NEKO_DEBUG("B from worker");
    });
    worker.sendTask([]() {
        NEKO_DEBUG("C from worker");
        NEKO_DEBUG("Task from");
    });
    NEKO_DEBUG(worker.invokeQueued(add, 1, 3));
    NEKO_DEBUG(worker.invokeQueued(returnVector, 1, 6, 9, 114514, 996, 10086));

    NEKO_LOG("A");

    try {
        worker.invokeQueued([]() {
            throw int(114514);
        });
    }
    catch (int &value) {
        NEKO_DEBUG(value);
    }

    NEKO_DEBUG("H from main");

    auto str = std::vector<int> {1, 1, 114514};
    auto str2 = std::vector<std::list<std::string>> { {"A", "B", "C"}, {"1"}};
    auto c = std::string("str");
    NEKO_DEBUG(str);
    NEKO_DEBUG(str2);
    NEKO_DEBUG(c);
    NEKO_DEBUG(114514.0f);
    NEKO_DEBUG("AAA");
    NEKO_DEBUG(std::chrono::system_clock::now());

    std::string array[] = {"A", "V", "D", "E"};
    std::string *ptr = (std::string *) 114514;

    NEKO_DEBUG(array);
    NEKO_DEBUG(ptr);

    auto en = MyEnum::MyValues;
    NEKO_DEBUG(en);
    en = MyEnum(-1);
    NEKO_DEBUG(en);

    NEKO_LOG_INFO("Enum current value is {}, type is {}", en, typeid(en));
    NEKO_LOG_INFO("Vector is {}, Vector::empty() = {}", str, str.empty());

    std::variant<int, float, std::optional<std::string> > var = std::optional<std::string>("A");
    NEKO_DEBUG(var);

    NEKO_DEBUG(std::make_tuple(114514, 10086.0, std::string("FIX")));

    std::map<int, std::string> mp;
    mp[0] = "a";
    mp[1145] = "a";
    mp[124] = "a";
    mp[555] = "a";
    mp[666] = "a";

    std::function<void()> fn = []() {};
    NEKO_DEBUG(mp);
    NEKO_DEBUG(typeid(NekoAV::Thread));

    NEKO_DEBUG(TestFlag::A | TestFlag::B);

    NEKO_DEBUG(NekoAV::GetTicks());
    NEKO_DEBUG(NekoAV::SleepFor(10));
    NEKO_DEBUG(NekoAV::GetTicks());

    NEKO_DEBUG(sizeof(std::string));
    NEKO_DEBUG(sizeof(std::vector<int>));
}