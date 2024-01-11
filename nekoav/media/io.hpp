#pragma once

#include "../defs.hpp"
#include <string>
#include <cstdio>
#include <span>

struct AVIOContext;

NEKO_NS_BEGIN

class IOStream {
public:
    /**
     * @brief Mark a provider as a fallback of (could not found scheme or fould not find proto)
     * 
     */
    constexpr static std::string_view AnyProtocol = {};
    /**
     * @brief Flags Of Stream
     * 
     */
    enum Flags : int32_t {
        None     = 0 << 0,
        Seekable = 1 << 0,
        Writable = 1 << 1,
        Readable = 1 << 2,
        Readwrite = Writable | Readable,
        All       = Seekable | Readwrite,
    };

    virtual ~IOStream() = default;

    virtual int64_t write(const void* data, size_t size) = 0;
    virtual int64_t read(void* data, size_t size) = 0;
    virtual int64_t seek(int64_t offset, int whence) = 0;
    virtual int64_t size() const = 0;
    virtual int32_t flags() const = 0;

    inline  int64_t tell() { return seek(0, SEEK_CUR); }

    inline  bool    isSeekable() const { return flags() & Seekable; }
    inline  bool    isWritable() const { return flags() & Writable; }
    inline  bool    isReadable() const { return flags() & Readable; }

    NEKO_API
    static Box<IOStream> fromFile(const char *file, const char *mode);
    NEKO_API
    static Box<IOStream> fromFILE(FILE *file, bool own = false);
    NEKO_API
    static Box<IOStream> fromMemory(const void *data, size_t size);
    NEKO_API
    static Box<IOStream> open(std::string_view url, const Properties *options =nullptr);

    // Registers
    template <typename T>
    static void registers(std::string_view proto) {
        registers(proto, make<T>);
    }
    NEKO_API
    static void registers(std::string_view proto, Box<IOStream> (*)(std::string_view url, const Properties *props));
private:
    template <typename T>
    static Box<IOStream> make(std::string_view url, const Properties *options) {
        return std::make_unique<T>(url, options);
    }
protected:
    IOStream() = default;
};

/**
 * @brief A dynmaic buffer
 * 
 */
class DynBuffer : public IOStream {
public:
    /**
     * @brief Get the data readed
     * 
     * @return std::span<uint8_t> 
     */
    virtual std::span<uint8_t> data() = 0;

    static Box<DynBuffer> create();
};

NEKO_NS_END