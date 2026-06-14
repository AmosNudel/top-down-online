#ifndef NET_STREAM_SOCKET_H
#define NET_STREAM_SOCKET_H

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using NetSocket = SOCKET;
constexpr NetSocket kInvalidSocket = INVALID_SOCKET;
inline int netSocketLastError() { return WSAGetLastError(); }
inline void netSocketClose(NetSocket s)
{
    if (s != kInvalidSocket)
        closesocket(s);
}
inline bool netSocketWouldBlock(int err)
{
    return err == WSAEWOULDBLOCK;
}
inline void netSocketShutdown(NetSocket s)
{
    if (s != kInvalidSocket)
        shutdown(s, SD_BOTH);
}
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using NetSocket = int;
constexpr NetSocket kInvalidSocket = -1;
inline int netSocketLastError() { return errno; }
inline void netSocketClose(NetSocket s)
{
    if (s != kInvalidSocket)
        close(s);
}
inline bool netSocketWouldBlock(int err)
{
    return err == EWOULDBLOCK || err == EAGAIN;
}
#ifndef SD_BOTH
#define SD_BOTH SHUT_RDWR
#endif
inline void netSocketShutdown(NetSocket s)
{
    if (s != kInvalidSocket)
        shutdown(s, SD_BOTH);
}
#endif

struct NetSocketInit
{
    NetSocketInit()
    {
#if defined(_WIN32)
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    ~NetSocketInit()
    {
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};

inline bool netSetNonBlocking(NetSocket socket)
{
#if defined(_WIN32)
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0)
        return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

#endif // NET_STREAM_SOCKET_H
