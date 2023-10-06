#include "../nekoav/compute/opencl.hpp"
#include "../nekoav/log.hpp"

using namespace NEKO_NAMESPACE::OpenCL;

int main() {
    Library cl;

    Context ctxt(Device::systemDefault(cl));
    auto dev = ctxt.device();

    NEKO_DEBUG(dev.name());
    NEKO_DEBUG(dev.vendor());
    NEKO_DEBUG(dev.version());

    for (auto platform : Platform::all(cl)) {
        NEKO_DEBUG(platform.name());
        NEKO_DEBUG(platform.vendor());
        NEKO_DEBUG(platform.version());
        NEKO_DEBUG(platform.extensions());
        NEKO_DEBUG(platform.hasExtension(DX11_INTEROP_EXT));
        NEKO_DEBUG(platform.hasExtension(DX10_INTEROP_EXT));
        NEKO_DEBUG(platform.hasExtension(DX9_INTEROP_EXT));
        NEKO_DEBUG(platform.hasExtension(GL_INTEROP_EXT));

#ifdef _WIN32
        D3D11ExtLibrary ext(cl, platform.get());
        NEKO_DEBUG(ext.clCreateFromD3D11BufferKHR);
        NEKO_DEBUG(ext.clCreateFromD3D11Texture2DKHR);
        NEKO_DEBUG(ext.clCreateFromD3D11Texture3DKHR);
        NEKO_DEBUG(ext.clEnqueueAcquireD3D11ObjectsKHR);
        NEKO_DEBUG(ext.clEnqueueReleaseD3D11ObjectsKHR);
#endif
    }

    NEKO_DEBUG(ctxt.properties());
    CommandQueue queue(ctxt, dev);
    auto program = Program::createWithSource(ctxt, R"(
        __kernel void test(__global float* output, int w) {
            output[0] = w;
        }
    )");
    if (!program.valid()) {
        NEKO_DEBUG("Could not create program");
        NEKO_DEBUG(program.buildLog());
        return EXIT_FAILURE;
    }
    Buffer buffer(ctxt, CL_MEM_READ_WRITE, 1024);
    NEKO_DEBUG(program.build());
    NEKO_DEBUG(program.il());
    auto kernel = program.createKernel("test");
    kernel.setArg(0, buffer);
    kernel.setArg(1, 1);
    auto ret = queue.enqueueTask(kernel);
    ret = queue.finish();
    float rValue;
    ret = queue.enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(rValue), &rValue);
    NEKO_DEBUG(ret);
    NEKO_DEBUG(rValue);
}