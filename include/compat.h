#ifndef COMPAT_H
#define COMPAT_H

#ifdef _WIN32
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef CONDITION_VARIABLE_INIT
#define CONDITION_VARIABLE_INIT NULL
#endif
#ifndef CONDITION_VARIABLE_LOCKMODE_SHARED
#define CONDITION_VARIABLE_LOCKMODE_SHARED 1
#endif
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif
#ifndef _access_s
#define _access_s(path, mode) _access(path, mode)
#endif

#include "third_party/mingw.thread.h"
#include "third_party/mingw.mutex.h"
#include "third_party/mingw.condition_variable.h"

namespace std {
    using mingw_stdthread::thread;
    using mingw_stdthread::mutex;
    using mingw_stdthread::recursive_mutex;
    using mingw_stdthread::lock_guard;
    using mingw_stdthread::unique_lock;
    using mingw_stdthread::condition_variable;
    using mingw_stdthread::condition_variable_any;
    namespace this_thread {
        using namespace mingw_stdthread::this_thread;
    }
}
#endif // _WIN32

#endif // COMPAT_H
