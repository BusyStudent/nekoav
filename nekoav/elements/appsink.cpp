#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../detail/queue.hpp"
#include "../factory.hpp"
#include "../pad.hpp"
#include "appsink.hpp"

NEKO_NS_BEGIN

class AppSinkImpl final : public Template::GetImpl<AppSink> {
public:
    AppSinkImpl() {
        mSink = addInput("sink");
        mSink->setCallback(std::bind(&AppSinkImpl::processInput, this, std::placeholders::_1));
    }
    Error processInput(View<Resource> resourceView) {
        mQueue.emplace(resourceView->shared_from_this());
        return Error::Ok;
    }
    Error pull(Arc<Resource> *resource, int timeout) override {
        if (!resource) {
            return Error::InvalidArguments;
        }
        if (mQueue.wait(resource, timeout)) {
            return Error::Ok;
        }
        return Error::TemporarilyUnavailable;
    }
private:
    BlockingQueue<Arc<Resource> > mQueue;
    Pad                          *mSink = nullptr;
};

NEKO_REGISTER_ELEMENT(AppSink, AppSinkImpl);

NEKO_NS_END
