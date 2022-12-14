#pragma once

#include <iostream>

#define _STRINGIZE_I(x) #x
#define _STRINGIZE(x) _STRINGIZE_I(x)

inline void _iouring_assert(const char *condition, const char *message, const char *fileline)
{
    std::cerr << "[" << fileline << "] "
              << "Assertion `" << condition << "` failed.\n"
              << message << std::endl;
    std::abort();
}

inline void _iouring_assert(const char *condition, const std::string &message, const char *fileline)
{
    _iouring_assert(condition, message.c_str(), fileline);
}

#ifdef NDEBUG
#    define IOURING_ASSERT(condition, message) static_cast<void>(0)
#else
#    define IOURING_ASSERT(condition, message)                                                                                             \
        static_cast<bool>(condition) ? static_cast<void>(0) : _iouring_assert(#condition, message, __FILE__ ":" _STRINGIZE(__LINE__))
#endif
