#pragma once

#include "../detail/base.hpp"
#include "../context.hpp"
#include "opengl.hpp"

NEKO_NS_BEGIN

namespace _abiv1 {

template <typename ...Ts>
class OpenGLImpl : public ThreadingExImpl<Ts...> {
public:
    OpenGLImpl() {

    }
    ~OpenGLImpl() {
        NEKO_ASSERT(this->state() == State::Null);
    }
protected:
    /**
     * @brief Called after OpenGL was initialized
     * 
     * @return Error 
     */
    virtual Error onGLInitialize() {
        return Error::Ok;
    }
    /**
     * @brief Called before OpenGL Teardown
     * 
     * @return Error 
     */
    virtual Error onGLTeardown() {
        return Error::Ok;
    }
    /**
     * @brief Get the OpenGL Context
     * 
     * @return GLContext* 
     */
    GLContext *glcontext() const noexcept {
        return mGLContext;
    }
    /**
     * @brief Make the context current
     * 
     * @return true 
     * @return false 
     */
    bool makeCurrent() {
        return mGLContext->makeCurrent();
    }
    /**
     * @brief Done the current context
     * 
     */
    void doneCurrent() {
        return mGLContext->doneCurrent();
    }
    /**
     * @brief Get the OpenGL Loader, pass it to GLFunction::load
     * 
     * @return auto 
     */
    auto functionLoader() const noexcept {
        return std::bind(&GLContext::getProcAddress, mGLContext, std::placeholders::_1);
    }
private:
    /**
     * @brief Do initalize of OpenGL
     * 
     * @return Error 
     */
    Error onInitialize() override final {
        mGLContext = mController->allocContext();
        if (!mGLContext) {
            return Error::OutOfMemory;
        }
        return onGLInitialize();
    }
    /**
     * @brief Do cleanup of OpenGL
     * 
     * @return Error 
     */
    Error onTeardown() override final {
        auto err = onGLTeardown();
        mController->freeContext(mGLContext);
        mGLContext = nullptr;
        return err;
    }
    Error onLoop() override final {
        return ThreadingElementDelegate::onLoop();
    }
    Thread *allocThread() override final {
        auto ctxt = Element::context();
        if (!ctxt) {
            return nullptr;
        }
        mController = ctxt->queryObject<GLController>();
        if (!mController) {
            return nullptr;
        }
        return mController->allocThread();
    }
    void freeThread(Thread *thread) override final {
        mController->freeThread(thread);
        mController = nullptr;
    }

    GLController *mController = nullptr;
    GLContext    *mGLContext = nullptr;
};

}

using _abiv1::OpenGLImpl;

NEKO_NS_END