#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class AppSink : public Element {
public:
    /**
     * @brief Pull the sink to get resource in queue
     * 
     * @param resource 
     * @param timeout The timeout in millseconds 0 on no-blovcking, -1 on INF 
     * @return Error 
     */
    virtual Error pull(Arc<Resource> *resource, int timeout = 0) = 0;

    /**
     * @brief Pull the sink to get resource in queue
     * 
     * @tparam T The type of resource
     * @param resource 
     * @param timeout 
     * @return Error Error::UnsupportedResource on type dismatch
     */
    template <typename T>
    inline Error pull(Arc<T> *resource, int timeout = 0) {
        Arc<Resource> res;
        auto err = pull(&res, timeout);
        if (err != Error::Ok) {
            return err;
        }
        *resource = std::dynamic_pointer_cast<T>(res);
        if (resource->get()) {
            return Error::Ok;
        }
        return Error::UnsupportedResource;
    }
};

NEKO_NS_END