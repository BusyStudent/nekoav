#pragma once

#include "../detail/base.hpp"
#include "../context.hpp"
#include "opengl.hpp"

NEKO_NS_BEGIN

class GLFunctions; //< Function for Auto Load

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
     * @brief Get the OpenGL Controller
     * 
     * @return GLController* 
     */
    GLController *glcontroller() const noexcept {
        return mController;
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
        return std::bind(&GLContext::getProcAddress<void*>, mGLContext, std::placeholders::_1);
    }
    /**
     * @brief Create a OpenGL Frame by giveing args
     * 
     * @param width 
     * @param height 
     * @param format 
     * @return Arc<GLFrame> 
     */
    auto createGLFrame(int width, int height, GLFrame::Format format) {
        return CreateGLFrame(mController, width, height, format);
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
        mGLContext->makeCurrent();
        // Auto Load OpenGL Function
        if constexpr (std::is_base_of_v<GLFunctions, OpenGLImpl<Ts...> >) {
            this->load(functionLoader());
        }
        return onGLInitialize();
    }
    /**
     * @brief Do cleanup of OpenGL
     * 
     * @return Error 
     */
    Error onTeardown() override final {
        mGLContext->makeCurrent();
        auto err = onGLTeardown();
        mGLContext->doneCurrent();
        
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
            // No controller, add one
            auto controller = CreateGLController();
            mController = controller.get();
            ctxt->addObject(std::move(controller));
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