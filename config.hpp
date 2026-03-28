
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define _LAUNCHERQIU_OS_NAME "windows"
    #ifdef _WIN64
        #define _LAUNCHERQIU_OS_ARCH "x86_64"
    #else
        #define _LAUNCHERQIU_OS_ARCH "x86"
    #endif
    #define _LAUNCHERQIU_CLASSPATH_SEPARATOR ";"
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        #define _LAUNCHERQIU_OS_NAME "unknown"
    #elif TARGET_OS_IPHONE
        #define _LAUNCHERQIU_OS_NAME "unknown"
    #elif TARGET_OS_MAC
        #define _LAUNCHERQIU_OS_NAME "osx"
    #else
        #define _LAUNCHERQIU_OS_NAME "unknown"
    #endif
    #define _LAUNCHERQIU_OS_ARCH "unknown"
    #define _LAUNCHERQIU_CLASSPATH_SEPARATOR ":"
#elif __linux__
    #define _LAUNCHERQIU_OS_NAME "linux"
    #ifdef __i386__
        #define _LAUNCHERQIU_OS_ARCH "x86"
    #elif __x86_64__
        #define _LAUNCHERQIU_OS_ARCH "x86_64"
    #else
        #define _LAUNCHERQIU_OS_ARCH "unknown"
    #endif
    #define _LAUNCHERQIU_CLASSPATH_SEPARATOR ":"
//#elif __unix__
//#elif defined(_POSIX_VERSION)
#else
    #define _LAUNCHERQIU_OS_NAME "unknown"
    #define _LAUNCHERQIU_OS_ARCH "unknown"
    #define _LAUNCHERQIU_CLASSPATH_SEPARATOR ":"
#endif

#define LAUNCHERQIU_OS_NAME std::string(_LAUNCHERQIU_OS_NAME)
#define LAUNCHERQIU_OS_ARCH std::string(_LAUNCHERQIU_OS_ARCH)
#define LAUNCHERQIU_CLASSPATH_SEPARATOR std::string(_LAUNCHERQIU_CLASSPATH_SEPARATOR)
