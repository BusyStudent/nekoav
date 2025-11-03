#include <nekoav/elements/video.hpp>
#include "internal.hpp"

namespace nekoav {
    
struct VideoDecoder::Impl {
    AVCodecContext *ctxt = nullptr;
};


} // namespace nekoav
