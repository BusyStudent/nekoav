#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../pad.hpp"
#include "appsrc.hpp"

NEKO_NS_BEGIN

class AppSourceImpl final : public Template::GetImpl<AppSource> {
public:
    AppSourceImpl() {
        mSrc = addInput("src");
    }
    Error push(View<Resource> resource) override {
        if (!resource) {
            return Error::InvalidArguments;
        }
        return mSrc->push(resource);
    }
private:
    Pad                          *mSrc = nullptr;
};

NEKO_REGISTER_ELEMENT(AppSource, AppSourceImpl);

NEKO_NS_END
