#pragma once

#include "../libc.hpp"
#include "io.hpp"
#include <map>

NEKO_NS_BEGIN

namespace {

class CFileWrapper final : public IOStream {
public:
    CFileWrapper(FILE *f, bool close) : mFile(f), mClose(close) {

    }
    ~CFileWrapper() {
        if (mClose) {
            ::fclose(mFile);
        }
    }
    int32_t flags() const noexcept {
        return Flags::Readwrite | Flags::Seekable;
    }
    int64_t read(void *buffer, size_t size) override {
        if (auto ret = ::fread(buffer, 1, size, mFile); ret > 0) {
            return ret;
        }
        return -1;
    }
    int64_t write(const void *buffer, size_t size) override {
        if (auto ret = ::fwrite(buffer, 1, size, mFile); ret > 0) {
            return ret;
        }
        return -1;
    }
    int64_t seek(int64_t offset, int whence) override {
        return ::fseek(mFile, offset, whence);
    }
    int64_t size() const override {
        if (mSize >= 0) {
            return mSize;
        }
        auto current = ::ftell(mFile);
        ::fseek(mFile, 0, SEEK_END);
        mSize = ::ftell(mFile);
        ::fseek(mFile, current, SEEK_SET);
        return mSize;
    }
private:
    FILE *mFile = nullptr;
    bool mClose = false;
    mutable int64_t mSize = -1;
};

}

using IOFactory = std::map<std::string_view, Box<IOStream> (*)(std::string_view url, const Properties *)>;

static IOFactory & GetIOFactory() {
    static IOFactory f;
    return f;
};
static Box<IOStream> TryAnyProtocol(IOFactory &f, std::string_view url, const Properties *options) {
    auto iter = f.find(IOStream::AnyProtocol);
    if (iter == f.end()) {
        return nullptr;
    }
    return iter->second(url, options);
}

Box<IOStream> IOStream::open(std::string_view url, const Properties *options) {
    if (url.empty()) {
        return nullptr;
    }

    auto &factory = GetIOFactory();
    // First split scheme
    auto pos = url.find("://");
    if (pos == std::string_view::npos) {
        // Maybe local file
        if (url.front() == '/' || url.front() == '\\') {
            return IOStream::fromFile(std::string(url).c_str(), "rb");
        }
        return TryAnyProtocol(factory, url, options);
    }
    auto scheme = url.substr(0, pos);
    // Then find factory
    auto iter = factory.find(scheme);
    if (iter == factory.end()) {
        return TryAnyProtocol(factory, url, options);
    }
    return iter->second(url, options);
}
Box<IOStream> IOStream::fromFile(const char *path, const char *mode) {
    auto f = libc::u8fopen(path, mode);
    if (!f) {
        return nullptr;
    }
    return IOStream::fromFILE(f, true);
}
Box<IOStream> IOStream::fromFILE(FILE *f, bool owned) {
    if (f == nullptr) {
        return nullptr;
    }
    return std::make_unique<CFileWrapper>(f, owned);
}

void IOStream::registers(std::string_view proto, Box<IOStream> (*fn)(std::string_view url, const Properties *props)) {
    GetIOFactory().insert(std::make_pair(proto, fn));
}

NEKO_NS_END