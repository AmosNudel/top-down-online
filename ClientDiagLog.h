#ifndef CLIENT_DIAG_LOG_H
#define CLIENT_DIAG_LOG_H

#include "raylib.h"
#include <cstdarg>
#include <cstdio>
#include <string>

inline const char *ClientDiagLogPath()
{
    static std::string path;
    if (!path.empty())
        return path.c_str();

    const char *dir = GetApplicationDirectory();
    if (dir && dir[0])
    {
        path = dir;
        const char last = path.back();
        if (last != '/' && last != '\\')
            path += '/';
        path += "game.log";
        return path.c_str();
    }

    path = "game.log";
    return path.c_str();
}

inline void ClientDiagLog(const char *fmt, ...)
{
#if defined(PLATFORM_WEB)
    (void)fmt;
#else
    FILE *f = std::fopen(ClientDiagLogPath(), "a");
    if (!f)
        return;

    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fputc('\n', f);
    std::fflush(f);
    std::fclose(f);
#endif
}

#endif // CLIENT_DIAG_LOG_H
