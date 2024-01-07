#define _NEKO_SOURCE
#include "../elements.hpp"

NEKO_NS_BEGIN

/**
 * @brief A Video Filter by using convolution kernel
 * 
 */
class KernelFilter : public Element {
public:
    /**
     * @brief Set the Kernel object
     * 
     * @code {.cpp}
     *  setKernel(array, 3, 3); //< 3x3 
     * @endcode
     * 
     * 
     * @param kernel 
     * @param kernelCol 
     * @param kernelRow 
     */
    virtual Error setKernel(const double *kernel, int kernelCol, int kernelRow) = 0;

    inline Error setEdgeDetectKernel() {
        const double kernel[3][3] = {
            {-1, -1, -1},
            {-1,  8, -1},
            {-1, -1, -1}
        };
        return setKernel(kernel);
    }
    inline Error setSharpenKernel() {
        const double kernel[3][3] = {
            {0.0,  -0.5,  0.0},
            {-0.5,  3,   -0.5},
            {0.0,  -0.5,  0.0}
        };
        return setKernel(kernel);
    }

    template <size_t Col, size_t Row>
    inline Error setKernel(const double (&kernel)[Col][Row]) {
        return setKernel(reinterpret_cast<const double *>(kernel), Col, Row);
    }
};

NEKO_NS_END