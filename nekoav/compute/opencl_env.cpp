#define _NEKO_SOURCE
#include "opencl_env.hpp"
#include "../media/sharing.hpp"
#include "../elements.hpp"
#include "../context.hpp"
#include "../log.hpp"

NEKO_NS_BEGIN

OpenCLContext::OpenCLContext() {
    auto device = cl::Device::getDefault();
    mContext = cl::Context(device);
    mCommandQueue = cl::CommandQueue(mContext, device);

    if (mCommandQueue.get() == nullptr) {
        return;
    }
    auto version = device.getInfo<CL_DEVICE_VERSION>();
    mExtensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    ::sscanf(version.c_str(), "OpenCL %d.%d", &mMajorVersion, &mMinorVersion);

    NEKO_LOG("OCL Device: {}", device.getInfo<CL_DEVICE_NAME>());
    NEKO_LOG("OCL Version: {}", device.getInfo<CL_DEVICE_VERSION>());
}
OpenCLContext::~OpenCLContext() {

}

Arc<OpenCLContext> OpenCLContext::create(Element *element) {
    auto elementContext = element->context();
    if (!elementContext) {
        return OpenCLContext::create();
    }
    auto context = new OpenCLContext();
    elementContext->addObjectView<OpenCLContext>(context);
    return Arc<OpenCLContext>(context, [elementContext](OpenCLContext *ctxt) {
        elementContext->removeObject<OpenCLContext>(ctxt);
        delete ctxt;
    });
}
Arc<OpenCLContext> OpenCLContext::create() {
    return MakeShared<OpenCLContext>();
}




NEKO_NS_END