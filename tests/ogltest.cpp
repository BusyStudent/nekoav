#include "../nekoav/elements/appsink.hpp"
#include "../nekoav/elements/appsrc.hpp"
#include "../nekoav/opengl/upload.hpp"
#include "../nekoav/opengl/download.hpp"
#include "../nekoav/opengl/shader.hpp"
#include "../nekoav/media/frame.hpp"
#include "../nekoav/media/reader.hpp"
#include "../nekoav/media/writer.hpp"
#include "../nekoav/pipeline.hpp"
#include "../nekoav/factory.hpp"
#include "../nekoav/context.hpp"

using namespace NEKO_NAMESPACE;

int main() {
    auto pipeline = CreateElement<Pipeline>();
    auto upload = CreateElement<GLUpload>();
    auto download = CreateElement<GLDownload>();
    auto kernel = CreateElement<GLKernel>();
    auto sink = CreateElement<AppSink>();
    auto src = CreateElement<AppSource>();

    // kernel->setKernel(GLKernel::createEdgeDetectKernel());

    auto err = pipeline->addElements(src, upload, kernel, download, sink);
    err = LinkElements(src, upload, kernel, download, sink);

    err = pipeline->setState(State::Running);

    // Send a frame to pipeline
    auto reader = CreateMediaReader();
    if (!reader) {
        //< No Puglin impl it
        return EXIT_FAILURE;
    }
    err = reader->openUrl("https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm");
    for (auto stream : reader->streams()) {
        if (stream.type == StreamType::Video) {
            err = reader->selectStream(stream.index);
            break;
        }
    }
    Arc<MediaFrame> frame;
    int index;
    for (int i = 0; i < 100; i++) {
        if (reader->readFrame(&frame, &index) == Error::Ok) {
            src->push(frame); //< Send to pipeline
            sink->pull(&frame, -1); //< Pull the result
        }
    }
    WriteVideoFrameToFile(frame, "glout.png");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    pipeline->setState(State::Null);
}