#pragma once

#include "../defs.hpp"
#include <functional>

#ifdef _WIN32
struct ID3D11Device;
struct ID3D11DeviceContext;
#endif

NEKO_NS_BEGIN

class MediaFrame;

#ifdef _WIN32
/**
 * @brief A Interface for sharing Direct3D 11 implment 
 * 
 */
class D3D11SharedContext : public std::enable_shared_from_this<D3D11SharedContext> {
public:
    /**
     * @brief Convert a frame (SW or another D3D11 Frame into wanted hardware frame)
     * 
     * @param frame 
     * @param wantedDXGIFormat The wanted format (0 on default format, commonly RGBA)
     * @return Error 
     */
    virtual Error convertFrame(Arc<MediaFrame> *frame, int wantedDXGIFormat = 0) = 0;
    /**
     * @brief Get the D3D11 Device Context
     * 
     * @return ID3D11DeviceContext* 
     */
    virtual ID3D11DeviceContext *context() const = 0;
    /**
     * @brief Get the D3D11 Device
     * 
     * @return ID3D11Device* 
     */
    virtual ID3D11Device        *device() const = 0;
    /**
     * @brief Lock the shared context before you want to any things
     * 
     */
    virtual void  lock() = 0;
    /**
     * @brief Unlock it
     * 
     */
    virtual void  unlock() = 0;
};
#endif

/**
 * @brief A Interface proveided OpenGL Sharing
 * 
 */
class OpenGLSharing {
public:
    /**
     * @brief Invoke callable at GL Thread, it should block until the fn was called
     * 
     * @param fn 
     */
    virtual void invokeAtGLThread(std::function<void()> &&fn) = 0;
};

#ifdef _WIN32
extern NEKO_API Arc<D3D11SharedContext> GetD3D11SharedContext(Element *element);
extern NEKO_API Arc<D3D11SharedContext> GetD3D11SharedContext(Context *context);
#endif

NEKO_NS_END