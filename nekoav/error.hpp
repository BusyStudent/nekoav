#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

enum class Error : int {
    Ok = 0,              //< No Error
    NoLink,              //< Pad is unlinked
    NoImpl,              //< User doesnot impl it
    NoStream,            //< No Media Strean founded
    NoCodec,             //< Mo Media Codec founded
    UnsupportedFormat,   //< Unsupported media format
    UnsupportedResource, //< Unsupported Resource type 
    InvalidArguments,
    InvalidTopology,
    InvalidState,
    OutOfMemory,
    Async,                  //< This operation is a asynchronous
    Internal,               //< Internal Error
    TemporarilyUnavailable, //< This operation is TemporarilyUnavailable, try again later
    FileNotFound          , //< This file is not founded
    

    Unknown,
    NumberOfErrors, //< Numbers of Error code
};

NEKO_NS_END