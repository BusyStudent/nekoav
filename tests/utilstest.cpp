#include "../nekoav/backtrace.hpp"
#include "../nekoav/threading.hpp"
#include "../nekoav/log.hpp"
#include <map>

using namespace std::chrono_literals;

enum MyEnum {
    A, B, C, D, E, F, MyValues
};

template <char ...Fmts>
void Call() {

}

int main() {
    NekoAV::Thread worker;
    worker.postTask([]() {
        NEKO_DEBUG("A from worker");
    });
    worker.postTask([]() {
        NEKO_DEBUG("B from worker");
    });
    worker.sendTask([]() {
        NEKO_DEBUG("C from worker");
        NEKO_DEBUG("Task from");
    });
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

    NEKO_LOG("Enum current value is {}, type is {}", en, typeid(en));
    NEKO_LOG("Vector is {}, Vector::empty() = {}", str, str.empty());

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
}