#define _NEKO_SOURCE

#include "../compute/opencl_env.hpp"
#include "../media/frame.hpp"
#include "../detail/base.hpp"
#include "../threading.hpp"
#include "../factory.hpp"
#include "../libc.hpp"
#include "../log.hpp"
#include "../time.hpp"
#include "../pad.hpp"
#include "filters.hpp"

// Check OpenMP
#if defined(_OPENMP)
    #include <omp.h>
    #define OMP_Pragma(x) _Pragma(x)
#else
    #define OMP_Pragma(x)
#endif


NEKO_NS_BEGIN

static auto filterCommonCode = R"(
__kernel void convolution(__read_only image2d_t input, __write_only image2d_t output, sampler_t imageSampler) {
    
}
)";

class KernelFilterImpl final : public Impl<KernelFilter> {
public:
    KernelFilterImpl() {
        mSink->addProperty(Properties::PixelFormatList, {PixelFormat::RGBA});
    }
    Error onInitialize() override {
        mApply = &KernelFilterImpl::_applyOnCPU;

        // OpenCL parts
        mOpenCLContext = OpenCLContext::create(this);
        if (mOpenCLContext) {
            mApply = &KernelFilterImpl::_applyOnOpenCL;
            if (!mKernel.empty()) {
                _compileCLProgram();
            }
        }
        return Error::Ok;
    }
    Error onTeardown() override {
        // Cleanup opencl
        if (mOpenCLContext) {
            mOpenCLContext->releaseObjects(&mSrcImage, &mDstImage, &mSampler, &mProgram, &mCLKernel);
            mOpenCLContext.reset();
        }

        std::free(mRGBABuffer);
        mRGBABuffer = nullptr;
        mRGBABufferSize = 0;
        return Error::Ok;
    }

    Error setKernel(const double *kernel, int kernelCol, int kernelRow) override {
        // Check is valid kernel
        if (kernelCol <= 0 || kernelRow <= 0 || kernel == nullptr) {
            return Error::InvalidArguments;
        }
        mKernelCol = kernelCol;
        mKernelRow = kernelRow;
        mKernel.assign(kernel, kernel + kernelCol * kernelRow);

        if (mOpenCLContext) {
            _compileCLProgram();
        }
        return Error::Ok;
    }
    Error _applyOnCPU(MediaFrame *frame) {
        if (auto err = frame->makeWritable(); !err) {
            return Error::Unknown;
        }
        uint8_t *dst = (uint8_t*) frame->data(0);
        int width = frame->width();
        int height = frame->height();
        _resizeBuffer(width * height * 4);


        ::memcpy(mRGBABuffer, dst, width * height * 4);

        // Begin 
        double sum[4] {0, 0, 0, 0};
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t *src = mRGBABuffer + (y * width + x) * 4;

                OMP_Pragma("omp parallel for")
                for (int i = 0; i < mKernelRow; i++) {
                    for (int j = 0; j < mKernelCol; j++) {
                        int xx = x + j - mKernelCol / 2;
                        int yy = y + i - mKernelRow / 2;

                        // Mirror coord if out of 
                        if (xx < 0) {
                            xx = -xx;
                        } 
                        else if (xx >= width) {
                            xx = 2 * width - 1 - xx;
                        }
                        if (yy < 0) {
                            yy = -yy;
                        } 
                        else if (yy >= height) {
                            yy = 2 * height - 1 - yy;
                        }

                        if (xx >= 0 && xx < width && yy >= 0 && yy < height) {
                            uint8_t *src2 = mRGBABuffer + (yy * width + xx) * 4;
                            sum[0] += mKernel[i * mKernelCol + j] * src2[0];
                            sum[1] += mKernel[i * mKernelCol + j] * src2[1];
                            sum[2] += mKernel[i * mKernelCol + j] * src2[2];
                            sum[3] += mKernel[i * mKernelCol + j] * src2[3];
                        }
                    }
                }

                for (auto &v : sum) {
                    v = std::clamp(v, 0.0, 255.0);
                }

                dst[(y * width + x) * 4 + 0] = sum[0];
                dst[(y * width + x) * 4 + 1] = sum[1];
                dst[(y * width + x) * 4 + 2] = sum[2];
                // dst[(y * width + x) * 4 + 3] = sum[3];

                for (auto &v : sum) {
                    v = 0;
                }
            }
        }
        return Error::Ok;
    }
    Error _applyOnOpenCL(MediaFrame *frame) {
        frame->makeWritable();

        cl_int err = 0;
        auto &commandQueue = mOpenCLContext->commandQueue();
        auto &ctxt = mOpenCLContext->context();
        if (mSrcImage.get() == nullptr) {
            cl::ImageFormat format(CL_RGBA, CL_UNORM_INT8);
            mSrcImage = cl::Image2D(ctxt, CL_MEM_READ_ONLY, format, frame->width(), frame->height(), frame->linesize(0), nullptr, &err);
            mDstImage = cl::Image2D(ctxt, CL_MEM_WRITE_ONLY, format, frame->width(), frame->height(), frame->linesize(0), nullptr, &err);
            mSampler = cl::Sampler(ctxt, CL_FALSE, CL_ADDRESS_MIRRORED_REPEAT, CL_FILTER_NEAREST, &err);
        }

        // Update to buffer
        cl::array<cl::size_type, 2> origin = {0, 0};
        cl::array<cl::size_type, 2> region = {cl::size_type(frame->width()), cl::size_type(frame->height())};
        err = commandQueue.enqueueWriteImage(mSrcImage, CL_TRUE, origin, region, 0, 0, frame->data(0));
        // err = commandQueue.enqueueReadImage(mSrcImage, CL_TRUE, origin, region, 0, 0, frame->data(0));

        cl::KernelFunctor<cl::Image2D, cl::Image2D, cl::Sampler> fn(mCLKernel);

        return Error::Ok;
    }
    Error onSinkPush(View<Pad>, View<Resource> resource) {
        if (mKernel.empty()) {
            // Just forward
            return pushTo(mSrc, resource);
        }
        auto frame = resource.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        NEKO_TRACE_TIME(duration) {
            if (auto err = std::invoke(mApply, this, frame); err != Error::Ok) {
                return err;
            }
        }
        mTimeCosted = duration;
        return pushTo(mSrc, resource);
    }
    void _resizeBuffer(size_t size) {
        if (mRGBABufferSize < size) {
            mRGBABuffer = (uint8_t *) std::realloc(mRGBABuffer, size);
            mRGBABufferSize = size;
        }
    }
    Error _compileCLProgram() {
        std::string programCode;
        cl_int err = 0;
        libc::sprintf(&programCode, "__constant float convKernel[%d][%d] = {\n", mKernelRow, mKernelCol);
        for (int i = 0; i < mKernelRow; i++) {
            programCode.append("{ ");
            for (int j = 0; j < mKernelCol; j++) {
                libc::sprintf(&programCode, "%.2f, ", mKernel[i * mKernelCol + j]);
            }
            // Remove ", "
            programCode.pop_back();
            programCode.pop_back();
            programCode.append("}, ");
        }
        // Remove ", "
        programCode.pop_back();
        programCode.pop_back();
        programCode.append("};\n");
        libc::sprintf(&programCode, "__constant int kernelWidth = %d; __constant int kernelHeight = %d;", mKernelCol, mKernelRow);

        // Add Common Part
        programCode.append(filterCommonCode);

        mProgram = cl::Program(mOpenCLContext->context(), programCode);
        if (mProgram.build() != CL_SUCCESS) {
#ifndef NDEBUG
            auto log = mProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>();
            for (auto& [dev, str] : log) {
                NEKO_LOG("Build Fail log => {}", str);
            }
#endif
            return Error::External;
        }
        mCLKernel = cl::Kernel(mProgram, "convolution", &err);
        if (err != CL_SUCCESS) {
            return Error::External;
        }
        return Error::Ok;
    }
private:
    Vec<double> mKernel;
    int mKernelCol = 0;
    int mKernelRow = 0;

    Pad *mSink = addInput("sink");
    Pad *mSrc = addOutput("src");

    int64_t mTimeCosted = 0;

    // OpenCL part
    Arc<OpenCLContext> mOpenCLContext; //< A sharing context for compute
    cl::Image2D        mSrcImage;
    cl::Image2D        mDstImage;
    cl::Sampler        mSampler;
    cl::Program        mProgram;
    cl::Kernel         mCLKernel;

    // Plain CPU
    uint8_t *mRGBABuffer = nullptr;
    size_t   mRGBABufferSize = 0;

    Error (KernelFilterImpl::*mApply)(MediaFrame *) = nullptr;
};

NEKO_REGISTER_ELEMENT(KernelFilter, KernelFilterImpl);

NEKO_NS_END