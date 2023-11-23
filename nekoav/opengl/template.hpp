#pragma once

#include "../elements.hpp"
#include "../context.hpp"
#include "opengl.hpp"

NEKO_NS_BEGIN

inline namespace _abvi {
namespace Template {

class UniqueGLContext {
public:
    UniqueGLContext(GLContext *ctxt) : mCtxt(ctxt) {
        mCtxt->makeCurrent();
    }
    ~UniqueGLContext() {
        mCtxt->doneCurrent();
    }
private:
    GLContext *mCtxt = nullptr;
};

template <typename ...Ts>
class GetGLImpl : public Ts ... {
public:
    Error changeState(StateChange change) override {
        // Preinit
        if (change == StateChange::Initialize) {
            auto ctxt = this->context();
            if (!ctxt) {
                return Error::InvalidContext;
            }
            mController = ctxt->queryObject<GLController>();
            if (!mController) {
                return Error::InvalidContext;
            }
            mThread = mController->allocThread();
            NEKO_ASSERT(mThread);
        }

        // Call thread
        Error ret;
        mThread->sendTask([&, this]() {
            ret = onChangeState(change);
            if (ret == Error::Ok) {
                this->mState = GetTargetState(change);
            }
        });

        // Deinit
        if  (change == StateChange::Teardown || (change == StateChange::Initialize && ret != Error::Ok)) {
            NEKO_ASSERT(mController);
            if (!mController) {
                return Error::InvalidContext;
            }
            mController->freeThread(mThread);
            mThread = nullptr;
        }
        return ret;
    }

    virtual Error onInitialize() { return Error::Ok; }
    virtual Error onPrepare(){ return Error::Ok; }
    virtual Error onRun() { return Error::Ok; }
    virtual Error onPause() { return Error::Ok; }
    virtual Error onStop() { return Error::Ok; }
    virtual Error onTeardown() { return Error::Ok; }

    Error onChangeState(StateChange change) {
        switch (change) {
            case StateChange::Initialize : {
                mContext = mController->allocContext();
                if (!mContext) {
                    return Error::Unknown;
                }
                UniqueGLContext guard(mContext);
                return onInitialize();
            }
            case StateChange::Prepare    : {
                UniqueGLContext guard(mContext);
                return onPrepare();
            }
            case StateChange::Run        : {
                UniqueGLContext guard(mContext);
                return onRun();
            }
            case StateChange::Pause      : {
                UniqueGLContext guard(mContext);
                return onPause();
            }
            case StateChange::Stop       : {
                UniqueGLContext guard(mContext);
                return onStop();
            }
            case StateChange::Teardown   : {
                mContext->makeCurrent();
                auto err = onTeardown();
                
                mController->freeContext(mContext);
                mContext = nullptr;
                return err;
            }
            default                      : return Error::InvalidArguments;
        }
    }

    GLContext *glcontext() const noexcept {
        return mContext;
    }
    Thread *thread() const noexcept {
        return mThread;
    }

    // Memory allocation
    void *operator new(size_t size) noexcept {
        return libc::malloc(size);
    }
    void operator delete(void *ptr) noexcept {
        return libc::free(ptr);
    }
private:
    GLController *mController = nullptr;
    GLContext    *mContext = nullptr;
    Thread *mThread = nullptr;
};


}
}


NEKO_NS_END