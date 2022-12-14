#pragma once

#if defined(ENABLE_LOGGING)
#    if defined(IOURING_LOG_LEVEL)
#        define SPDLOG_ACTIVE_LEVEL IOURING_LOG_LEVEL
#    else
#        define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#    endif
#else
#    define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_OFF
#endif
#include <spdlog/async.h>
#include <spdlog/fmt/bundled/printf.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#if defined(ENABLE_LOGGING)
#    define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#    define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#    define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#    define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#    define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#else
#    define LOG_TRACE(...) (void)0
#    define LOG_DEBUG(...) (void)0
#    define LOG_INFO(...) (void)0
#    define LOG_WARN(...) (void)0
#    define LOG_ERROR(...) (void)0
#endif
