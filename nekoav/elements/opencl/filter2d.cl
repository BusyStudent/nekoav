__constant float convKernel [3][3] = {
    {-1, -1, -1},
    {-1,  9, -1},
    {-1, -1, -1}
};
__constant int kernelWidth = 3;
__constant int kernelHeight = 3;

__kernel void convolution(__read_only image2d_t input, __write_only image2d_t output, sampler_t imageSampler) {
    int x = get_global_id(0);
    int y = get_global_id(1);

    if (x >= get_image_width(input) || y >= get_image_height(input)) {
        return;
    }

    float4 sum = (float4)(0.0f);
    int2 coords;

    __attribute__((opencl_unroll_hint))
    for (int i = -kernelHeight/2; i <= kernelHeight/2; i++) {
        for (int j = -kernelWidth/2; j <= kernelWidth/2; j++) {
            coords = (int2)(x + j, y + i);
            sum += read_imagef(input, imageSampler, coords) * convKernel[i + kernelHeight/2][j + kernelWidth/2];
        }
    }
    // Ignore Alpha channel
    sum.w = 1.0f;

    write_imagef(output, (int2)(x, y), sum);
    return;
}

