#include "../nekoav/backtrace.hpp"
#include "../nekoav/threading.hpp"

int main() {
    NekoAV::Thread worker;
    worker.postTask([]() {
        puts("A from worker");
    });
    worker.postTask([]() {
        puts("B from worker");
    });
    worker.sendTask([]() {
        puts("C from worker");
    });
    puts("H from main");
}