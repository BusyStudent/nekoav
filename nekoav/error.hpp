#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

enum class Error : int {
    Ok = 0,              //< No Error
    NoConnection,        //< Pad is unlinked
    NoImpl,              //< User doesnot impl it
    NoStream,            //< No Media Strean founded
    NoCodec,             //< Mo Media Codec founded
    UnsupportedFormat,   //< Unsupported media format
    UnsupportedResource, //< Unsupported Resource type 
    InvalidArguments,
    InvalidTopology,
    InvalidState,
    OutOfMemory,
    Internal,            //< Internal Error

    Unknown,
    NumberOfErrors, //< Numbers of Error code
};

NEKO_NS_END