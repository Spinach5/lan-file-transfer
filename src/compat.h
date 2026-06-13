/* Cross-platform compatibility layer */
#ifndef COMPAT_H
#define COMPAT_H

#if defined(_WIN32) || defined(_WIN64)
/* ── Windows ──────────────────────────────────────────────── */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <process.h>

/* Map POSIX to Windows */
#define sleep(s)          Sleep((s)*1000)
#define usleep(us)        Sleep((us)/1000)
#define close(fd)         closesocket(fd)
#define strcasecmp        _stricmp
#define strncasecmp       _strnicmp
#define getcwd(b,s)       _getcwd(b,s)
#define chdir(p)          _chdir(p)
#define mkdir(p,m)        _mkdir(p)
#define unlink(p)         _unlink(p)
#define stat              _stat
#define fstat             _fstat

typedef int socklen_t;
typedef SOCKET socket_t;
#define INVALID_FD        INVALID_SOCKET
#define SOCKET_ERROR_VAL  SOCKET_ERROR

/* gettimeofday replacement */
#include <sys/timeb.h>
static inline int compat_gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    struct _timeb tb;
    _ftime(&tb);
    tv->tv_sec = (long)tb.time;
    tv->tv_usec = tb.millitm * 1000;
    return 0;
}
#define gettimeofday compat_gettimeofday

/* No getopt_long on MSVC — use a simple shim for MinGW; MSVC needs replacement */
#ifndef __GNUC__
/* MSVC: minimal getopt replacement */
extern int optind;
extern char *optarg;
int getopt(int argc, char *const argv[], const char *optstring);
#else
/* MinGW has getopt_long */
#include <getopt.h>
#endif

/* Socket startup/shutdown */
static inline int net_startup(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa);
}
static inline void net_cleanup(void) { WSACleanup(); }
#define SOCKET_INIT()    net_startup()
#define SOCKET_QUIT()    net_cleanup()

/* Non-blocking socket setup */
static inline int sock_set_nonblock(socket_t fd) {
    unsigned long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
}

/* Missing POSIX types */
#define ssize_t SSIZE_T

#else
/* ── Linux / Unix ─────────────────────────────────────────── */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <getopt.h>

typedef int socket_t;
#define INVALID_FD        (-1)
#define SOCKET_ERROR_VAL  (-1)
#define SOCKET_INIT()     0
#define SOCKET_QUIT()     do{}while(0)

static inline int sock_set_nonblock(socket_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

#endif /* COMPAT_H */
