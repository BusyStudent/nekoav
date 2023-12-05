#include "../nekoav/opengl/upload.hpp"
#include "../nekoav/opengl/opengl.hpp"
#include "../nekoav/pipeline.hpp"
#include "../nekoav/factory.hpp"
#include "../nekoav/context.hpp"

using namespace NEKO_NAMESPACE;

int main() {
    auto pipeline = CreateElement<Pipeline>();
    auto upload = CreateElement<GLUpload>();
    auto glcontroller = CreateGLController();

    pipeline->addElements(upload);
    pipeline->context()->addObject<GLController>(std::move(glcontroller));

    pipeline->setState(State::Running);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    pipeline->setState(State::Null);
}