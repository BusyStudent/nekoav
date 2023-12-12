#define _NEKO_SOURCE
#include "videosrc.hpp"
#include "../detail/base.hpp"
#include "../threading.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../format.hpp"

NEKO_NS_BEGIN

class TestVideoSourceImpl final : public ThreadingImpl<TestVideoSource> {
public:
    TestVideoSourceImpl() {
        mSrc = addOutput("src");
    }
    ~TestVideoSourceImpl() {

    }
    Error onLoop() override {
        while (!stopRequested()) {
            thread()->waitTask();
            while (state() == State::Running) {
                thread()->waitTask(1000 / 30);
                pushFrame();
            }
        }
        return Error::Ok;
    }
    void pushFrame() {
        auto frame = CreateVideoFrame(PixelFormat::RGBA, mWidth, mHeight);
        frame->makeWritable();

        uint32_t *data = (uint32_t *)frame->data(0);
        ::memset(data, 0xFF, frame->linesize(0) * frame->height());
        mClock += 1000 / 30;

        frame->setDuration(1000 / 30);
        frame->setTimestamp(mClock);

        pushTo(mSrc, frame);
    }
    void setOutputSize(int width, int height) override {
        mWidth = width;
        mHeight = height;
    }
private:
    Pad *mSrc;
    double mClock = 0.0;
    int mWidth = 1920;
    int mHeight = 1080;
};

NEKO_REGISTER_ELEMENT(TestVideoSource, TestVideoSourceImpl);

NEKO_NS_END