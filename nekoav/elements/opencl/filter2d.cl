__constant float convKernel [3][3] = {
    {-1, -1, -1},
    {-1,  9, -1},
    {-1, -1, -1}
};
__constant int kernelWidth = 3;
__constant int kernelHeight = 3;

__kernel void convolution(__read_only image2d_t input, __write_only image2d_t output, sampler_t imageSampler) {
    int2 coords = (int2)(get_global_id(0), get_global_id(1));
}

