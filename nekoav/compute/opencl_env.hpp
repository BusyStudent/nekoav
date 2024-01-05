#pragma once

#include <cl/opencl.hpp>
#include "../elements.hpp"
#include "../context.hpp"
#include "../defs.hpp"

NEKO_NS_BEGIN

/**
 * @brief A Shared OpenCL Context in pipelines
 * 
 */
class OpenCLContext final : public std::enable_shared_from_this<OpenCLContext> {
public:
    OpenCLContext();
    ~OpenCLContext();

    /**
     * @brief Get the cl::Context ref
     * 
     * @return cl::Context& 
     */
    cl::Context      &context() noexcept;
    /**
     * @brief Get the command ref
     * 
     * @return cl::CommandQueue& 
     */
    cl::CommandQueue &commandQueue() noexcept;
    /**
     * @brief A Helper to release OpenCL object
     * 
     * @tparam Args... 
     * @param v The object you want to release
     */
    template <typename ...Args>
    void  releaseObjects(Args ...args);

    void *operator new(size_t size);
    void  operator delete(void *data);

    /**
     * @brief Create a context, and set the ownship to element's context
     * 
     * @param element 
     * @return Arc<OpenCLContext> 
     */
    static Arc<OpenCLContext> create(Element *element);
    /**
     * @brief Create a ioslate context
     * 
     * @return Arc<OpenCLContext> 
     */
    static Arc<OpenCLContext> create();
private:
    cl::Context      mContext;
    cl::CommandQueue mCommandQueue;
};


// -- IMPL
inline OpenCLContext::OpenCLContext() {
    auto device = cl::Device::getDefault();
    mContext = cl::Context(device);
    mCommandQueue = cl::CommandQueue(mContext, device);
}
inline OpenCLContext::~OpenCLContext() {

}

inline cl::Context &OpenCLContext::context() noexcept {
    return mContext;
}
inline cl::CommandQueue &OpenCLContext::commandQueue() noexcept {
    return mCommandQueue;
}

inline Arc<OpenCLContext> OpenCLContext::create(Element *element) {
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
inline Arc<OpenCLContext> OpenCLContext::create() {
    return MakeShared<OpenCLContext>();
}

template <typename ...Args>
inline void OpenCLContext::releaseObjects(Args ...args) {
    ((*args = {}), ...);
}

inline void *OpenCLContext::operator new(size_t size) {
    return libc::malloc(size);
}
inline void  OpenCLContext::operator delete(void *ptr) {
    return libc::free(ptr);
}

NEKO_NS_END