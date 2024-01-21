#define CL_HPP_ENABLE_EXCEPTIONS
#include "../nekoav/hwcontext/opencl.hpp"
#include "../nekoav/log.hpp"
#include <CL/opencl.hpp>


void doCompute(cl::Context &ctxt, cl::CommandQueue &queue) {
    cl::Program program(ctxt, R"(
        __kernel void mul(__global const float* a, __global const float* b, __global const float* c, __global float *d) {
            int i = get_global_id(0);
            d[i] = a[i] * b[i] * c[i];
        }
    )");
    program.build();

    cl::KernelFunctor<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer> compute(program, "mul");

    float arrayA [10] { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1};
    float arrayB [10] { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    float arrayC[10] {100, 200, 300, 400, 500, 600, 700, 800, 900, 114514};
    float arrayD[10];

    cl::Buffer bufferA(ctxt, CL_MEM_READ_ONLY, sizeof(arrayA));
    cl::Buffer bufferB(ctxt, CL_MEM_READ_ONLY, sizeof(arrayB));
    cl::Buffer bufferC(ctxt, CL_MEM_READ_ONLY, sizeof(arrayC));
    cl::Buffer bufferD(ctxt, CL_MEM_WRITE_ONLY, sizeof(arrayD));

    queue.enqueueWriteBuffer(bufferA, CL_TRUE, 0, sizeof(arrayA), arrayA);
    queue.enqueueWriteBuffer(bufferB, CL_TRUE, 0, sizeof(arrayB), arrayB);
    queue.enqueueWriteBuffer(bufferC, CL_TRUE, 0, sizeof(arrayC), arrayC);
    compute(cl::EnqueueArgs(queue, cl::NDRange(10)), bufferA, bufferB, bufferC, bufferD);
    queue.enqueueReadBuffer(bufferD, CL_TRUE, 0, sizeof(arrayD), arrayD);

    // Print result
    NEKO_DEBUG(arrayD);
}

int main() {
    auto ctxt = NekoAV::OpenCLContext::create();
    doCompute(ctxt->context(), ctxt->commandQueue());
}