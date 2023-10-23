#pragma once

#include "../utils.hpp"
#include "../defs.hpp"
#include "opencl_map.hpp"
#include <CL/opencl.h>
#include <functional>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

#ifdef _WIN32
    #define NEKO_OPENCL_DLL_PATH "OpenCL.dll"
    #include <CL/cl_d3d11.h>
#elif __linux__
    #define NEKO_OPENCL_DLL_PATH "libOpenCL.so.1"
#else
    #error "Unsupported platform"
#endif

#ifdef NEKO_CXX20
    #include <source_location>
#endif

#ifdef NEKOCL_STATIC_IMPORT
    #define nekocl_library_path(path)
    #define nekocl_import(name) neko_import_symbol_static(name, name)
#else
    #define nekocl_library_path(path) neko_library_path(path)
    #define nekocl_import(name) neko_import(name)
#endif

// OpenCL Ext import
#define nekocl_ext_import4(name) name##_fn name = (name##_fn) mCl->clGetExtensionFunctionAddressForPlatform(mId, #name);

NEKO_NS_BEGIN

namespace OpenCL {

// EXT Constant
inline constexpr std::string_view GL_INTEROP_EXT = "cl_khr_gl_sharing";
inline constexpr std::string_view DX11_INTEROP_EXT = "cl_khr_d3d11_sharing";
inline constexpr std::string_view DX10_INTEROP_EXT = "cl_khr_d3d10_sharing";
inline constexpr std::string_view DX9_INTEROP_EXT = "cl_khr_dx9_media_sharing";
inline constexpr std::string_view NVDX11_INTEROP_EXT = "cl_nv_d3d11_sharing";
inline constexpr std::string_view NVDX10_INTEROP_EXT = "cl_nv_d3d10_sharing";

/**
 * @brief Alloc once only array
 * 
 * @tparam T 
 */
template <typename T>
class DynArray {
public:
    explicit DynArray(std::unique_ptr<T []> &&data, size_t n) : mData(std::move(data)), mSize(n) { }
    DynArray() = default;
    DynArray(const DynArray &) = default;
    DynArray(DynArray &&) = default;
    ~DynArray() = default;

    static DynArray<T> make(size_t n) {
        if (n == 0) {
            return DynArray();
        }
        return DynArray<T>(std::make_unique<T []>(n), n);
    }

    void resize(size_t n) {
        if (n != 0) {
            mData = std::make_unique<T []>(n);            
        }
        else {
            mData.reset();
        }
        mSize = n;
    }

    auto begin() const noexcept {
        return mData.get();
    }

    auto end() const noexcept {
        return mData.get() + mSize;
    }

    auto get() const noexcept {
        return mData.get();
    }
    auto data() noexcept {
        return mData.get();
    }

    size_t size() const noexcept {
        return mSize;
    }
    bool contains(const T &v) const noexcept {
        for (const auto &each : *this) {
            if (::memcmp(&each, &v, sizeof(T)) == 0) {
                return true;
            }
        }
        return false;
    }

    auto &operator [](size_t i) const noexcept {
        return mData[i];
    }

    auto toVector() const noexcept {
        return std::vector<T>(begin(), end());
    }
private:
    std::unique_ptr<T []> mData { };
    size_t                mSize { };
};

template <typename T>
class Span {
public:
    Span() = default;
    Span(const T *p, size_t n) : mData(p), mSize(n) { }
    Span(const std::vector<T> &vec) : mData(vec.data()), mSize(vec.size()) { }
    Span(const DynArray<T> &arr) : mData(arr.get()), mSize(arr.size()) { }
    template <size_t N>
    Span(const T (&arr)[N]) : mData(arr), mSize(N) { }

    auto data() const noexcept {
        return mData;
    }
    size_t size() const noexcept {
        return mSize;
    }
    auto begin() const noexcept {
        return mData;
    }
    auto end() const noexcept {
        return mData + mSize;
    }
    auto operator [](size_t i) const noexcept {
        return mData[i];
    }
private:
    const T *mData { };
    size_t   mSize { };
};

/**
 * @brief CL Library function pointer collection
 * 
 */
class Library {
public:
    nekocl_library_path(NEKO_OPENCL_DLL_PATH);

    // Core
    nekocl_import(clGetPlatformInfo);
    nekocl_import(clGetPlatformIDs);
    nekocl_import(clGetDeviceIDs);
    nekocl_import(clGetDeviceInfo);
    nekocl_import(clGetContextInfo);
    nekocl_import(clGetCommandQueueInfo);
    nekocl_import(clGetProgramBuildInfo);
    nekocl_import(clGetProgramInfo);
    nekocl_import(clGetImageInfo);
    nekocl_import(clGetEventInfo);
    nekocl_import(clCreateContext);
    nekocl_import(clCreateCommandQueue);
    nekocl_import(clCreateBuffer);
    nekocl_import(clCreateProgramWithSource);
#ifdef CL_VERSION_2_1
    nekocl_import(clCreateProgramWithIL);
#endif
    nekocl_import(clCreateUserEvent);
    nekocl_import(clBuildProgram);
    nekocl_import(clCreateKernel);
    nekocl_import(clWaitForEvents);
    nekocl_import(clEnqueueNDRangeKernel);
    nekocl_import(clEnqueueReadBuffer);
    nekocl_import(clEnqueueWriteBuffer);
    nekocl_import(clEnqueueTask);
    nekocl_import(clReleaseCommandQueue);
    nekocl_import(clReleaseContext);
    nekocl_import(clReleaseKernel);
    nekocl_import(clReleaseMemObject);
    nekocl_import(clReleaseProgram);
    nekocl_import(clReleaseEvent);
    nekocl_import(clRetainCommandQueue);
    nekocl_import(clRetainContext);
    nekocl_import(clRetainProgram);
    nekocl_import(clRetainKernel);
    nekocl_import(clRetainEvent);
    nekocl_import(clRetainMemObject);
    nekocl_import(clSetKernelArg);
    nekocl_import(clSetEventCallback);
    nekocl_import(clFinish);
    nekocl_import(clCreateImage);
    nekocl_import(clCreateImage2D);

    nekocl_import(clGetSupportedImageFormats);
    nekocl_import(clGetExtensionFunctionAddressForPlatform);

    // OGL Interop
    nekocl_import(clCreateFromGLBuffer);
    nekocl_import(clCreateFromGLTexture2D);
    nekocl_import(clCreateFromGLTexture3D);
    nekocl_import(clCreateFromGLTexture);
    nekocl_import(clGetGLObjectInfo);
    nekocl_import(clGetGLTextureInfo);
    nekocl_import(clEnqueueAcquireGLObjects);
    nekocl_import(clEnqueueReleaseGLObjects);
    nekocl_import(clGetGLContextInfoKHR);

    // My Helper
    
    /**
     * @brief Get CL Object Info template
     * 
     * @tparam T The container type
     * @tparam Func The func to get the info
     * @tparam Handle The object type
     * @tparam PName The query info type
     * @param func The func to get the info
     * @param handle The object
     * @param name The query info
     * @param err The error code pointer (default in nullptr)
     * @return T() 
     */
    template <typename T, typename Func, typename Handle, typename PName>
    auto nekoclGetObjectInfo(Func func, Handle handle, PName name, cl_int *err = nullptr) const {
        cl_int _err;
        if (!err) {
            err = &_err;
        }

        size_t n;
        *err = func(handle, name, 0, nullptr, &n);
        if (*err != CL_SUCCESS) {
            return T();
        }
        T buf;
        buf.resize(n);
        *err = func(handle, name, n, buf.data(), nullptr);
        if (*err != CL_SUCCESS) {
            return T();
        }
        return buf;
    }

    auto nekoclGetPlatformString(cl_platform_id platform, cl_platform_info pname, cl_int *err = nullptr) const {
        return nekoclGetObjectInfo<std::string>(clGetPlatformInfo, platform, pname, err);
    }
    auto nekoclGetDeviceString(cl_device_id device, cl_device_info pname, cl_int *err = nullptr) const {
        return nekoclGetObjectInfo<std::string>(clGetDeviceInfo, device, pname, err);
    }
    auto nekoclGetContextString(cl_context context, cl_context_info pname, cl_int *err = nullptr) const {
        return nekoclGetObjectInfo<std::string>(clGetContextInfo, context, pname, err);
    }
    auto nekoclGetPlatforms() const {
        cl_uint n;
        cl_int err;
        err = clGetPlatformIDs(0, nullptr, &n);
        auto buf = DynArray<cl_platform_id>::make(n);
        err = clGetPlatformIDs(n, buf.get(), nullptr);
        return buf;
    }
    auto nekoclGetDevices(cl_platform_id platform, cl_device_type type = CL_DEVICE_TYPE_ALL) const {
        cl_uint n;
        cl_int err;
        err = clGetDeviceIDs(platform, type, 0, nullptr, &n);
        auto buf = DynArray<cl_device_id>::make(n);
        err = clGetDeviceIDs(platform, type, n, buf.get(), nullptr);
        return buf;
    }
};

class ExtLibraryBase {
public:
    ExtLibraryBase(const Library *lib, cl_platform_id id) : mCl(lib), mId(id) { }
protected:
    const Library *mCl;
    cl_platform_id mId;
};

#ifdef _WIN32
/**
 * @brief DX11 Interop
 * 
 */
class D3D11ExtLibrary : public ExtLibraryBase {
public:
    D3D11ExtLibrary(const Library *lib, cl_platform_id id) : ExtLibraryBase(lib, id) { }
    D3D11ExtLibrary(const Library &lib, cl_platform_id id) : ExtLibraryBase(&lib, id) { }

    nekocl_ext_import4(clGetDeviceIDsFromD3D11KHR);
    nekocl_ext_import4(clCreateFromD3D11BufferKHR);
    nekocl_ext_import4(clCreateFromD3D11Texture2DKHR);
    nekocl_ext_import4(clCreateFromD3D11Texture3DKHR);
    nekocl_ext_import4(clEnqueueAcquireD3D11ObjectsKHR);
    nekocl_ext_import4(clEnqueueReleaseD3D11ObjectsKHR);
};
#endif

/**
 * @brief Base class for contain CL Library pointer and debug check
 * 
 */
class Base {
public:
    Base() = default;
    Base(const Library *c) : mCl(c) { }
    Base(const Library &c) : mCl(&c) { }
    Base(const Base &) = default;
    ~Base() = default;

    const Library *library() const {
        return mCl;
    }
protected:
    /**
     * @brief Check the errcode is CL_SUCCESS
     * 
     */
    static bool checkReturn(
        cl_int errcode
#if NEKO_CXX20 && !defined(NDEBUG)
        ,
        std::source_location loc = std::source_location::current()
#endif
        ) 
    {
        if (errcode == CL_SUCCESS) {
            return true;
        }
#ifndef NDEBUG
        const char *msg = GetErrorString(errcode);
#if NEKO_CXX20
        ::fprintf(stderr, "[%s:%d (%s)] CL ERROR : %s\n", loc.file_name(), int(loc.line()), loc.function_name(), msg);
#else
        ::fprintf(stderr, "CL ERROR : %s\n", msg);
#endif

#endif
        return false;
    }

    template <typename T>
    static T returnAndCheck(
        T &&v, 
        const cl_int &errcode
#if NEKO_CXX20 && !defined(NDEBUG)
        ,
        std::source_location loc = std::source_location::current()
#endif
    ) {

#if NEKO_CXX20 && !defined(NDEBUG)
        checkReturn(errcode, loc);
#else
        checkReturn(errcode);
#endif
        return std::forward<T>(v);
    }

    const Library *mCl = nullptr;
};

/**
 * @brief Wrapper for cl_platform_id
 * 
 */
class Platform final : public Base {
public:
    Platform() = default;
    Platform(const Library &library, cl_platform_id id) : Base(library), mId(id) { }
    Platform(const Platform &) = default;
    ~Platform() = default;

    auto name() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetPlatformString(mId, CL_PLATFORM_NAME, &err), err);
    }
    auto vendor() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetPlatformString(mId, CL_PLATFORM_VENDOR, &err), err);
    }
    auto version() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetPlatformString(mId, CL_PLATFORM_VERSION, &err), err);
    }
    auto profile() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetPlatformString(mId, CL_PLATFORM_PROFILE, &err), err);
    }
    auto extensions() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetPlatformString(mId, CL_PLATFORM_EXTENSIONS, &err), err);
    }
    bool hasExtension(std::string_view ext) const {
        return extensions().find(ext) != std::string::npos;
    }

    cl_platform_id get() const {
        return mId;
    }

    static DynArray<Platform> all(const Library &library) {
        auto arr = library.nekoclGetPlatforms();
        if (arr.size() == 0) {
            return DynArray<Platform>();
        }
        auto ret = DynArray<Platform>::make(arr.size());
        for (size_t i = 0;i < arr.size(); i++) {
            ret[i] = Platform(library, arr[i]);
        }
        return ret;
    }
private:
    cl_platform_id mId { };
};

/**
 * @brief Wrapper for cl_device_id
 * 
 */
class Device final : public Base {
public:
    Device() = default;
    Device(const Library &library, cl_device_id id) : Base(library), mId(id) { }
    Device(const Device &) = default;
    ~Device() = default;

    auto name() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetDeviceString(mId, CL_DEVICE_NAME, &err), err);
    }
    auto vendor() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetDeviceString(mId, CL_DEVICE_VENDOR, &err), err);
    }
    auto version() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetDeviceString(mId, CL_DEVICE_VERSION, &err), err);
    }
    auto profile() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetDeviceString(mId, CL_DEVICE_PROFILE, &err), err);
    }
    auto extensions() const {
        cl_int err;
        return returnAndCheck(mCl->nekoclGetDeviceString(mId, CL_DEVICE_EXTENSIONS, &err), err);
    }
    auto library() const {
        return mCl;
    }

    cl_device_id get() const {
        return mId;
    }

    static Device systemDefault(const Library &library) {
        auto platforms = library.nekoclGetPlatforms();
        if (platforms.size() == 0) {
            return Device();
        }
        // GPU First
        auto devices = library.nekoclGetDevices(platforms[0], CL_DEVICE_TYPE_GPU);
        if (devices.size() == 0) {
            return Device();
        }
        return Device(library, devices[0]);
    }
private:
    cl_device_id mId { };
};
/**
 * @brief Wrapper for cl_context
 * 
 */
class Context final : public Base {
public:
    explicit Context(const Device &device, const cl_context_properties *properties = nullptr) : Base(device.library()) {
        cl_int err;
        cl_device_id id = device.get();
        mContext = mCl->clCreateContext(properties, 1, &id, nullptr, nullptr, &err);

        checkReturn(err);
    }
    explicit Context(const Library &library, cl_context ctxt, bool retain = true) : Base(library), mContext(ctxt) {
        if (retain && mContext) {
            checkReturn(mCl->clRetainContext(mContext));
        }
    }
    Context() = default;
    Context(const Context &c) : Context(*c.library(), c.mContext) { } 
    Context(Context &&c) {
        mContext = c.mContext;
        c.mContext = nullptr;
    }
    ~Context() {
        reset();
    }

    auto properties() const {
        cl_int err;
        auto ret = mCl->nekoclGetObjectInfo<DynArray<cl_context_properties> >(mCl->clGetContextInfo, mContext, CL_CONTEXT_PROPERTIES, &err);
        checkReturn(err);
        return ret;
    }
    auto device() const {
        cl_device_id dev;
        if (checkReturn(mCl->clGetContextInfo(mContext, CL_CONTEXT_DEVICES, sizeof(dev), &dev, nullptr))) {
            return Device(*mCl, dev);
        }
        return Device();
    }
    bool valid() const noexcept {
        return mContext != nullptr;
    }

    cl_context get() const noexcept {
        return mContext;
    }

    Context &operator =(Context &&c) {
        if (this != &c) {
            reset();
            mContext = c.mContext;
            mCl = c.mCl;
            c.mContext = nullptr;
        }
        return *this;
    }
    Context &operator =(std::nullptr_t) {
        reset();
        return *this;
    }
    void reset(cl_context newContext = nullptr) {
        if (mCl && mContext) {
            checkReturn(mCl->clReleaseContext(mContext));
        }
        mContext = newContext;
    }
private:
    cl_context     mContext { };
};
/**
 * @brief Wrapper for cl_command_queue
 * 
 */
class CommandQueue final : public Base {
public:
    explicit CommandQueue(const Context &ctxt, const Device &dev, cl_command_queue_properties prop = 0) : Base(ctxt.library()){
        cl_int err;
        mQueue = mCl->clCreateCommandQueue(
            ctxt.get(),
            dev.get(),
            prop,
            &err
        );
        checkReturn(err);
    }
    explicit CommandQueue(const Library &library, cl_command_queue queue, bool retain = true) : Base(library), mQueue(queue) {
        if (retain && mQueue) {
            checkReturn(mCl->clRetainCommandQueue(mQueue));
        }
    }
    CommandQueue(const CommandQueue &q) : CommandQueue(*q.library(), q.mQueue) { }
    ~CommandQueue() {
        reset();
    }

    cl_command_queue get() const noexcept {
        return mQueue;
    }
    bool valid() const noexcept {
        return mQueue != nullptr;
    }
    bool finish() const {
        return checkReturn(mCl->clFinish(mQueue));
    }
    // bool enqueueNDRangeKernel(cl_kernel kernel, cl_uint workDim, ) {
    //     mCl->clEnqueueNDRangeKernel(mQueue, kernel, workDim);
    // }
    bool enqueueTask(cl_kernel kernel, Span<cl_event> waitEvents = { }, cl_event *event = nullptr) {
        return checkReturn(mCl->clEnqueueTask(mQueue, kernel, waitEvents.size(), waitEvents.data(), event));
    }
    bool enqueueReadBuffer(cl_mem buffer, cl_bool blocking, size_t offset, size_t size, void *ptr, Span<cl_event> waitEvents = { }, cl_event *event = nullptr) {
        return checkReturn(mCl->clEnqueueReadBuffer(mQueue, buffer, blocking, offset, size, ptr, waitEvents.size(), waitEvents.data(), event));
    }

    Device device() const {
        cl_device_id dev;
        if (checkReturn(mCl->clGetCommandQueueInfo(mQueue, CL_QUEUE_DEVICE, sizeof(cl_device_id), &dev, nullptr))) {
            return Device(*mCl, dev);
        }
        return Device();
    }
    Context context() const {
        cl_context ctxt;
        if (checkReturn(mCl->clGetCommandQueueInfo(mQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &ctxt, nullptr))) {
            return Context(*mCl, ctxt);
        }
        return Context();
    }

    CommandQueue &operator=(CommandQueue &&q) {
        if (this != &q) {
            reset();
            mCl = q.mCl;
            mQueue = q.mQueue;
            q.mQueue = nullptr;
        }
    }
    CommandQueue &operator =(std::nullptr_t) {
        reset();
        return *this;
    }
    void reset(cl_command_queue newQueue = nullptr) {
        if (mCl && mQueue) {
            checkReturn(mCl->clReleaseCommandQueue(mQueue));
        }
        mQueue = newQueue;
    }
private:
    cl_command_queue mQueue { };
};
class Kernel final : public Base {
public:
    explicit Kernel(const Library &library, cl_kernel kernel, bool retain = true) : Base(library), mKernel(kernel) {
        if (retain && kernel) {
            checkReturn(mCl->clRetainKernel(kernel));
        }
    }
    Kernel() = default;
    Kernel(const Kernel &k) : Kernel(*k.library(), k.mKernel) { }
    ~Kernel() {
        if (mCl && mKernel) {
            checkReturn(mCl->clReleaseKernel(mKernel));
        }
    }

    bool setArg(cl_uint index, size_t size, const void *value) const {
        return checkReturn(mCl->clSetKernelArg(mKernel, index, size, value));
    }
    template <typename T>
    bool setArg(cl_uint index, const T &value) {
        if constexpr (std::is_base_of_v<Base, T>) {
            auto v = value.get();
            return setArg(index, sizeof(v), &v);
        }
        else {
            return setArg(index, sizeof(T), &value);
        }
    }

    cl_kernel get() const noexcept {
        return mKernel;
    }
    operator cl_kernel() const noexcept {
        return mKernel;
    }
private:
    cl_kernel mKernel { };
};

/**
 * @brief Wrapper for cl_program
 * 
 */
class Program final : public Base {
public:
    explicit Program(const Context &ctxt, const char *sourceCode, size_t len = size_t(-1)) : Base(ctxt.library()) {
        cl_int err;
        if (len == size_t(-1)) {
            len = ::strlen(sourceCode);
        }

        mProgram = mCl->clCreateProgramWithSource(
            ctxt.get(),
            1, 
            &sourceCode,
            &len,
            &err
        );

        checkReturn(err);
    }
    explicit Program(const Library &library, cl_program program, bool retian = true) : Base(library), mProgram(program) {
        if (retian && mProgram) {
            checkReturn(mCl->clRetainProgram(mProgram));
        }
    }
    Program() = default;
    Program(const Program &p) : Program(*p.library(), p.mProgram) { }
    Program(Program &&p) {
        mProgram = p.mProgram;
        p.mProgram = nullptr;
    }
    ~Program() {
        if (mCl && mProgram) {
            checkReturn(mCl->clReleaseProgram(mProgram));
        }
    }

    bool build() {
        return checkReturn(mCl->clBuildProgram(mProgram, 0, nullptr, nullptr, nullptr, nullptr));
    }
    auto buildLog() const {
        size_t len;
        cl_device_id dev = device().get();
        cl_int err = mCl->clGetProgramBuildInfo(
            mProgram,
            dev,
            CL_PROGRAM_BUILD_LOG,
            0,
            nullptr,
            &len
        );
        if (!checkReturn(err)) {
            return std::string();
        }
        std::string log;
        log.resize(len);
        err = mCl->clGetProgramBuildInfo(
            mProgram,
            dev,
            CL_PROGRAM_BUILD_LOG,
            len,
            log.data(),
            nullptr
        );
        if (!checkReturn(err)) {
            return std::string();
        }
        return log;
    }

    cl_program get() const noexcept {
        return mProgram;
    }
    bool valid() const noexcept {
        return mProgram != nullptr;
    }
    Device device() const {
        cl_device_id id;
        if (!checkReturn(mCl->clGetProgramInfo(mProgram, CL_PROGRAM_DEVICES, sizeof(cl_device_id), &id, nullptr))) {
            return Device();
        }
        return Device(*library(), id);
    }

    Kernel createKernel(const char *kernelName) {
        cl_int err;
        cl_kernel k = mCl->clCreateKernel(mProgram, kernelName, &err);
        if (checkReturn(err)) {
            return Kernel(*library(), k, false);
        }
        return Kernel();
    }

#ifdef CL_VERSION_2_1
    auto il() const {
        cl_int err;
        auto buf = mCl->nekoclGetObjectInfo<DynArray<uint8_t> >(
            mCl->clGetProgramInfo,
            mProgram,
            CL_PROGRAM_IL,
            &err
        );
        if (checkReturn(err)) {
            return buf;
        }
        return DynArray<uint8_t>();
    }
#endif

    static Program createWithSource(const Context &ctxt, const char *sourceCode, size_t len = size_t(-1)) {
        return Program(ctxt, sourceCode, len);
    }
#ifdef CL_VERSION_2_1
    static Program createWithIL(const Context &ctxt, const void *il, size_t len) {
        cl_int err;
        auto program = ctxt.library()->clCreateProgramWithIL(ctxt.get(), il, len, &err);
        return Program(*ctxt.library(), program, false);
    }
#endif
    static Program createWithFile(const Context &ctxt, const char *path) {
        auto fp = ::fopen(path, "r");
        if (!fp) {
            return Program();
        }

        ::fseek(fp, 0, SEEK_END);
        auto len = ::ftell(fp);
        ::fseek(fp, 0, SEEK_SET);

        auto buffer = std::make_unique<char[]>(len);
        auto ok = (::fread(buffer.get(), 1, len, fp) == len);
        ::fclose(fp);

        if (!ok) {
            return Program();
        }

        return createWithSource(ctxt, buffer.get(), len);
    }
private:
    cl_program mProgram { };
};

/**
 * @brief Wrapper for cl_event
 * 
 */
class Event : public Base {
public:
    enum Status : cl_int {
        Compelete = CL_COMPLETE,
        Running = CL_RUNNING,
        Submitted = CL_SUBMITTED,
        Queued = CL_QUEUED,
    };

    explicit Event(const Library &library, cl_event mEvent, bool retain = true) : Base(library), mEvent(mEvent) {
        if (retain && mEvent) {
            checkReturn(mCl->clRetainEvent(mEvent));
        }
    }
    Event() = default;
    Event(const Event &event) : Event(*event.library(), event.mEvent) { }
    Event(Event &&event) : Base(event.library()), mEvent(event.mEvent) {
        event.mEvent = nullptr;
    } 
    ~Event() {
        if (mCl && mEvent) {
            checkReturn(mCl->clReleaseEvent(mEvent));
        }
    }

    bool wait() const {
        return checkReturn(mCl->clWaitForEvents(1, &mEvent));
    }

    cl_int status() const {
        cl_int s;
        checkReturn(mCl->clGetEventInfo(mEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(s), &s, nullptr));
        return s;
    }

    bool setCallback(std::function<void()> &&cb, cl_int commandExecCallackType = CL_COMPLETE) {
        auto fn = std::make_unique<std::function<void()>>(std::move(cb));
        auto clfn = [](cl_event, cl_int, void *fn) {
            std::unique_ptr<std::function<void()> > ptr(static_cast<std::function<void()>* >(fn));
            (*ptr)();
        };

        if (checkReturn(mCl->clSetEventCallback(mEvent, commandExecCallackType, clfn, fn.get()))) {
            fn.release(); // ownership is transferred to the library
            return true;
        }
        return false;
    }
protected:
    cl_event mEvent { };
};

/**
 * @brief Wrapper for user event
 * 
 */
class UserEvent : public Event {
public:
    explicit UserEvent(const Library &library, cl_event event, bool retain) : Event(library, event, retain) { }
    explicit UserEvent(const Context &context) {
        mCl = context.library();
        cl_int err;
        mEvent = mCl->clCreateUserEvent(context.get(), &err);
        checkReturn(err);
    }
    UserEvent() = default;
    UserEvent(const UserEvent &) = default;
    UserEvent(UserEvent &&) = default;
    ~UserEvent() = default;
};

/**
 * @brief Useful constant image format
 * 
 */
class ImageFormat final : public cl_image_format {
public:
    static constexpr auto RGBA32 = cl_image_format {CL_RGBA, CL_UNORM_INT8};
    static constexpr auto BGRA32 = cl_image_format {CL_BGRA, CL_UNORM_INT8};

    static constexpr auto RGBA64 = cl_image_format {CL_RGBA, CL_UNORM_INT16};
    static constexpr auto BGRA64 = cl_image_format {CL_BGRA, CL_UNORM_INT16};
};

/**
 * @brief Wrppaer for cl_mem
 * 
 */
class MemObject : public Base {
public:
    explicit MemObject(const Library &library, cl_mem mMem, bool retain = true) : Base(library), mMem(mMem) {
        if (retain && mMem) {
            checkReturn(mCl->clRetainMemObject(mMem));
        }
    }
    MemObject() = default;
    MemObject(const MemObject &other) : MemObject(*other.library(), other.mMem) { }
    ~MemObject() {
        if (mCl && mMem) {
            checkReturn(mCl->clReleaseMemObject(mMem));
        }
    }

    cl_mem get() const noexcept {
        return mMem;
    }
    operator cl_mem() const noexcept {
        return mMem;
    }
protected:
    cl_mem mMem { };
};

/**
 * @brief Wrapper for cl_mem
 * 
 */
class Buffer final : public MemObject {
public:
    explicit Buffer(const Library &library, cl_mem mMem, bool retain = true) : MemObject(library, mMem, retain) { 

    }
    explicit Buffer(const Context &ctxt, cl_mem_flags memFlags, size_t size, void *hostPtr = nullptr) {
        cl_int err;
        mCl = ctxt.library();
        mMem = mCl->clCreateBuffer(ctxt.get(), memFlags, size, hostPtr, &err);
        checkReturn(err);
    }
    Buffer() = default;
    Buffer(const Buffer &) = default;
    ~Buffer() = default;
};

class Image2D final : public MemObject {
public:
    explicit Image2D(const Library &library, cl_mem mMem, bool retain = true) : MemObject(library, mMem, retain) { 

    }
    explicit Image2D(const Context &ctxt, 
                     size_t w, size_t h, 
                     const cl_image_format &imageFormat, 
                     cl_mem_flags memFlags = CL_MEM_READ_WRITE, 
                     void *hostPtr = nullptr, 
                     size_t pitch = 0) 
    {
        mCl = ctxt.library();
        // clCreateImage2D()
        cl_int err;
        mMem = mCl->clCreateImage2D(ctxt.get(), memFlags, &imageFormat, w, h, pitch, hostPtr, &err);
        checkReturn(err);
    }

    size_t width() const {
        return _info<size_t>(CL_IMAGE_WIDTH);
    }
    size_t height() const {
        return _info<size_t>(CL_IMAGE_HEIGHT);
    }
    size_t depth() const {
        return _info<size_t>(CL_IMAGE_DEPTH);
    }
    cl_image_format format() const {
        return _info<cl_image_format>(CL_IMAGE_FORMAT);
    }

    static auto supportedFormats(const Context &ctxt, cl_mem_flags memFlags = CL_MEM_READ_WRITE) {
        auto cl = ctxt.library();
        cl_uint numFormats;
        auto err = cl->clGetSupportedImageFormats(ctxt.get(), memFlags, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, &numFormats);
        if (!checkReturn(err)) {
            return DynArray<cl_image_format>();
        }
        auto arr = DynArray<cl_image_format>::make(numFormats);
        err = cl->clGetSupportedImageFormats(ctxt.get(), memFlags, CL_MEM_OBJECT_IMAGE2D, numFormats, arr.data(), nullptr);
        if (!checkReturn(err)) {
            return DynArray<cl_image_format>();
        }
        return arr;
    }
    static bool IsFormatSupported(const Context &ctxt, const cl_image_format &format, cl_mem_flags memFlags = CL_MEM_READ_WRITE) {
        return supportedFormats(ctxt, memFlags).contains(format);
    }
private:
    template <typename T>
    T _info(cl_image_info ifo) const {
        T ret;
        if (checkReturn(mCl->clGetImageInfo(mMem, ifo, sizeof(T), &ret, nullptr))) {
            return ret;
        }
        return T();
    }
};

}
NEKO_NS_END