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
};

NEKO_NS_END