#include <nekoav/elements/playbin.hpp>
#include <nekoav/element.hpp>
#include <ilias/platform.hpp>
#include <ilias/signal.hpp>

using std::literals::operator""s;

void ilias_main(int argc, char **argv) {
    auto pipeline = std::make_shared<nekoav::Pipeline>();
    auto playbin = std::make_shared<nekoav::PlayBin>();

    pipeline->addElement(playbin);

    auto _ = co_await pipeline->setState(nekoav::State::Running);
    co_await ilias::sleep(10s);
    auto _2 = co_await pipeline->setState(nekoav::State::Null);
    co_return;
}