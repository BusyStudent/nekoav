#pragma once

#include <CL/opencl.hpp>
#include "../defs.hpp"

NEKO_NS_BEGIN

class OpenCLContextPrivate;
/**
 * @brief A Shared OpenCL Context in pipelines
 * 
 */
class NEKO_API OpenCLContext final : public std::enable_shared_from_this<OpenCLContext> {
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
     * @brief Get the version of OpenCL
     * 
     * @return std::pair<int, int> 
     */
    std::pair<int, int> version() const noexcept;
    /**
     * @brief Check has this extension
     * 
     * @param v 
     * @return true 
     * @return false 
     */
    bool  hasExtension(std::string_view v) const noexcept;
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
    int              mMajorVersion = 0;
    int              mMinorVersion = 0;
    std::string      mExtensions;
    
    // Private Data here
    Box<OpenCLContextPrivate> d;
};


// -- IMPL
inline cl::Context &OpenCLContext::context() noexcept {
    return mContext;
}
inline cl::CommandQueue &OpenCLContext::commandQueue() noexcept {
    return mCommandQueue;
}
inline std::pair<int, int> OpenCLContext::version() const noexcept {
    return std::make_pair(mMajorVersion, mMinorVersion);
}
inline bool OpenCLContext::hasExtension(std::string_view name) const noexcept {
    return mExtensions.find(name) != std::string::npos;
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

/**
 * @brief Get the Open CL Error Code Name
 * 
 * @param errc 
 * @return constexpr const char* 
 */
inline constexpr const char *GetOpenCLErrorCodeName(cl_uint errc) noexcept {
    #define NEKOCL_MAP(x) case x : return #x
    switch (errc) {
        NEKOCL_MAP(CL_SUCCESS);
        NEKOCL_MAP(CL_DEVICE_NOT_FOUND);
        NEKOCL_MAP(CL_DEVICE_NOT_AVAILABLE);
        NEKOCL_MAP(CL_COMPILER_NOT_AVAILABLE);
        NEKOCL_MAP(CL_MEM_OBJECT_ALLOCATION_FAILURE);
        NEKOCL_MAP(CL_OUT_OF_RESOURCES);
        NEKOCL_MAP(CL_OUT_OF_HOST_MEMORY);
        NEKOCL_MAP(CL_PROFILING_INFO_NOT_AVAILABLE);
        NEKOCL_MAP(CL_MEM_COPY_OVERLAP);
        NEKOCL_MAP(CL_IMAGE_FORMAT_MISMATCH);
        NEKOCL_MAP(CL_IMAGE_FORMAT_NOT_SUPPORTED);
        NEKOCL_MAP(CL_BUILD_PROGRAM_FAILURE);
        NEKOCL_MAP(CL_MAP_FAILURE);
#ifdef CL_VERSION_1_1
        NEKOCL_MAP(CL_MISALIGNED_SUB_BUFFER_OFFSET);
        NEKOCL_MAP(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
#endif

#ifdef CL_VERSION_1_2
        NEKOCL_MAP(CL_COMPILE_PROGRAM_FAILURE);
        NEKOCL_MAP(CL_LINKER_NOT_AVAILABLE);
        NEKOCL_MAP(CL_LINK_PROGRAM_FAILURE);
        NEKOCL_MAP(CL_DEVICE_PARTITION_FAILED);
        NEKOCL_MAP(CL_KERNEL_ARG_INFO_NOT_AVAILABLE);
#endif

        NEKOCL_MAP(CL_INVALID_VALUE);
        NEKOCL_MAP(CL_INVALID_DEVICE_TYPE);
        NEKOCL_MAP(CL_INVALID_PLATFORM);
        NEKOCL_MAP(CL_INVALID_DEVICE);
        NEKOCL_MAP(CL_INVALID_CONTEXT);
        NEKOCL_MAP(CL_INVALID_QUEUE_PROPERTIES);
        NEKOCL_MAP(CL_INVALID_COMMAND_QUEUE);
        NEKOCL_MAP(CL_INVALID_HOST_PTR);
        NEKOCL_MAP(CL_INVALID_MEM_OBJECT);
        NEKOCL_MAP(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
        NEKOCL_MAP(CL_INVALID_IMAGE_SIZE);
        NEKOCL_MAP(CL_INVALID_SAMPLER);
        NEKOCL_MAP(CL_INVALID_BINARY);
        NEKOCL_MAP(CL_INVALID_BUILD_OPTIONS);
        NEKOCL_MAP(CL_INVALID_PROGRAM);
        NEKOCL_MAP(CL_INVALID_PROGRAM_EXECUTABLE);
        NEKOCL_MAP(CL_INVALID_KERNEL_NAME);
        NEKOCL_MAP(CL_INVALID_KERNEL_DEFINITION);
        NEKOCL_MAP(CL_INVALID_KERNEL);
        NEKOCL_MAP(CL_INVALID_ARG_INDEX);
        NEKOCL_MAP(CL_INVALID_ARG_VALUE);
        NEKOCL_MAP(CL_INVALID_ARG_SIZE);
        NEKOCL_MAP(CL_INVALID_KERNEL_ARGS);
        NEKOCL_MAP(CL_INVALID_WORK_DIMENSION);
        NEKOCL_MAP(CL_INVALID_WORK_GROUP_SIZE);
        NEKOCL_MAP(CL_INVALID_WORK_ITEM_SIZE);
        NEKOCL_MAP(CL_INVALID_GLOBAL_OFFSET);
        NEKOCL_MAP(CL_INVALID_EVENT_WAIT_LIST);
        NEKOCL_MAP(CL_INVALID_EVENT);
        NEKOCL_MAP(CL_INVALID_OPERATION);
        NEKOCL_MAP(CL_INVALID_GL_OBJECT);
        NEKOCL_MAP(CL_INVALID_BUFFER_SIZE);
        NEKOCL_MAP(CL_INVALID_MIP_LEVEL);
        NEKOCL_MAP(CL_INVALID_GLOBAL_WORK_SIZE);
        NEKOCL_MAP(CL_INVALID_PROPERTY);
        NEKOCL_MAP(CL_INVALID_IMAGE_DESCRIPTOR);
        NEKOCL_MAP(CL_INVALID_COMPILER_OPTIONS);
        NEKOCL_MAP(CL_INVALID_LINKER_OPTIONS);
        NEKOCL_MAP(CL_INVALID_DEVICE_PARTITION_COUNT);
#ifdef CL_VERSION_2_0
        NEKOCL_MAP(CL_INVALID_PIPE_SIZE);
        NEKOCL_MAP(CL_INVALID_DEVICE_QUEUE);
#endif
#ifdef CL_VERSION_2_2
        NEKOCL_MAP(CL_INVALID_SPEC_ID);
        NEKOCL_MAP(CL_MAX_SIZE_RESTRICTION_EXCEEDED);
#endif
        default : return "CL_UNKNOWN_ERROR";
    }
}

NEKO_NS_END