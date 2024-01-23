#define _NEKO_SOURCE
#include "opencl.hpp"
#include "../elements.hpp"
#include "../context.hpp"
#include "../log.hpp"

#ifdef _WIN32
    #include <CL/cl_d3d11.h>
    #include <wrl/client.h>
#endif

NEKO_NS_BEGIN

class OpenCLContextPrivate {
public:

};

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

#ifndef NDEBUG
    NEKO_LOG("OCL Device: {}", device.getInfo<CL_DEVICE_NAME>());
    NEKO_LOG("OCL Version: {}", device.getInfo<CL_DEVICE_VERSION>());

    cl::vector<cl::ImageFormat> formats;
    mContext.getSupportedImageFormats(CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, &formats);

    // for (auto fmt : formats) {
    //     NEKO_LOG("OCL Supported ImageFormat: {}, {}", fmt.image_channel_data_type, fmt.image_channel_order);
    // }
    // NEKO_ASSERT(std::find(formats.begin(), formats.end(), cl::ImageFormat(CL_RGBA, CL_UNORM_INT8)) != formats.end());
#endif
}
OpenCLContext::~OpenCLContext() {
    d.reset();
}

Arc<OpenCLContext> OpenCLContext::create(Element *element) {
    auto elementContext = element->context();
    if (!elementContext) {
        return OpenCLContext::create();
    }
    if (auto ctxt = elementContext->queryObject<OpenCLContext>(); ctxt) {
        NEKO_LOG("OCL: Using Shared Context {}", ctxt);
        return ctxt->shared_from_this();
    }
    auto ctxt = OpenCLContext::create();
    elementContext->addObject<OpenCLContext>(ctxt);
    return ctxt;
}
Arc<OpenCLContext> OpenCLContext::create() {
    return MakeShared<OpenCLContext>();
}




NEKO_NS_END