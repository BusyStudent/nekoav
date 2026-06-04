#pragma once

#include <format>
#include <print>

#if defined(NEKOAV_USE_LOG)
    #include <spdlog/spdlog.h>
    #define NEKOAV_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
    #define NEKOAV_INFO(...) SPDLOG_INFO(__VA_ARGS__)
    #define NEKOAV_WARN(...) SPDLOG_WARN(__VA_ARGS__)
    #define NEKOAV_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
    #define NEKOAV_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
#else
    #define NEKOAV_DEBUG(...) do {} while(0)
    #define NEKOAV_INFO(...) do {} while(0)
    #define NEKOAV_WARN(...) do {} while(0)
    #define NEKOAV_ERROR(...) do {} while(0)
    #define NEKOAV_CRITICAL(...) do {} while(0)
#endif // NEKOAV_USE_LOG