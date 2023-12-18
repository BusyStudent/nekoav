#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

enum class Error : int {
    Ok = 0,              //< No Error
    NoLink,              //< Pad is unlinked
    NoImpl,              //< User doesnot impl it
    NoStream,            //< No Media Strean founded
    NoCodec,             //< Mo Media Codec founded
    UnsupportedMediaFormat, //< Unsupported media format
    UnsupportedPixelFormat, //< Unsupported pixel format
    UnsupportedSampleFormat, //< Unsupported sample format
    UnsupportedResource, //< Unsupported Resource type 
    InvalidArguments,
    InvalidTopology,
    InvalidContext,
    InvalidState,
    OutOfMemory,
    Async,                  //< This operation is a asynchronous
    Internal,               //< Internal Error
    TemporarilyUnavailable, //< This operation is TemporarilyUnavailable, try again later
    FileNotFound,           //< This file is not founded
    FileCorrupted,          //< This file is corrupted
    Interrupted,            //< This operation is interrupted by some reason
    EndOfFile,              //< This operation is reached the end of file

    External,               //< External Error, it come from external library
    Unknown,                //< Unknown Error
    NumberOfErrors, //< Numbers of Error code
};

NEKO_NS_END