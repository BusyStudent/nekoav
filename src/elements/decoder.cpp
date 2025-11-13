#include <nekoav/elements/decoder.hpp>
#include "internal.hpp"

namespace nekoav {

struct Decoder::Impl {
    AVCodecContext *ctxt = nullptr;
    AVFrame        *frame = nullptr;

    Impl() {
        frame = av_frame_alloc();
    }
    ~Impl() {
        avcodec_free_context(&ctxt);
        av_frame_free(&frame);
    }
};

Decoder::Decoder(std::string_view name) : 
    Element(name), 
    mInput(createInputPad("in")), 
    mOutput(createOutputPad("out")) 
{
    // Input accept both audio and video
    mInput.mutableCaps().insert(Caps::AudioPacket, {});
    mInput.mutableCaps().insert(Caps::VideoPacket, {});
    mInput.setPushCallback<&Decoder::onPadPush>(this);

    // Output
    mOutput.mutableCaps().insert(Caps::AudioRaw, {});
    mOutput.mutableCaps().insert(Caps::VideoRaw, {});
}

Decoder::~Decoder() {
    assert(!d); // Should be close onTeardown
}

auto Decoder::onPrepare() -> IoTask<void> {
    // Init codec here
    co_return {};
}

auto Decoder::onTeardown() -> IoTask<void> {
    d.reset();
    co_return {};
}

auto Decoder::onPadPush(Pad &pad, Sample::Ptr sample) -> IoTask<void> {
    if (!sample) { // Forward EOS
        co_return co_await mOutput.push(sample);
    }
    if (!d) { // The decoder is not ready
        co_return Err(Error::InvalidState);
    }
    if (!sample->isPacket()) {
        co_return Err(Error::UnsupportedSampleType);
    }
    // Begin process
    auto packet = sample->toPacket();
    auto res = co_await ilias::blocking([&]() {
        int res = avcodec_send_packet(d->ctxt, packet->get());
        if (res != 0) {
            return res;
        }
        return avcodec_receive_frame(d->ctxt, d->frame);
    });

    // Need more data
    if (res == AVERROR(EAGAIN)) {
        co_return {};
    }
    else if (res < 0) {
        co_return Err(error::fromFFmpeg(res));
    }

    // Create new frame
    auto frame = Frame::make(av_frame_clone(d->frame), packet->timeBase());
    co_return co_await mOutput.push(frame);
}


} // namespace nekoav