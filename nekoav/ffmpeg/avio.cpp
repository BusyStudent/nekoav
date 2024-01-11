#define _NEKO_SOURCE
#include "../media/io.hpp"
#include "ffmpeg.hpp"

NEKO_NS_BEGIN

namespace FFmpeg {

class AVIOStream final : public IOStream {
public:
    AVIOStream(AVIOContext *ctx) : mCtxt(ctx) {

    }
    ~AVIOStream() {
        avio_context_free(&mCtxt);
    }

    int64_t read(void *ptr, size_t size) override {
        return avio_read(mCtxt, (uint8_t *) ptr, size);
    }
    int64_t write(const void *ptr, size_t size) override {
        avio_write(mCtxt,(const uint8_t *) ptr, size);
        return size;
    }
    int64_t seek(int64_t offset, int whence) override {
        return avio_seek(mCtxt, offset, whence);
    }
    int32_t flags() const noexcept override {
        int32_t flags = Readable;
        if (mCtxt->write_flag) {
            flags |= Writable;
        }
        if (mCtxt->seekable) {
            flags |= Seekable;
        }
        return flags;
    }
    int64_t size() const override {
        return avio_seek(mCtxt, 0, AVSEEK_SIZE);
    }
private:
    AVIOContext *mCtxt;
};

// NEKO_CONSTRUCTOR(_avio_ctor) {

// }

}

NEKO_NS_END