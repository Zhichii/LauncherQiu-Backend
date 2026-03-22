
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define OS_NAME "windows"
    #ifdef _WIN64
        // pass
    #else
        // pass
    #endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        #error "Not supported. Please contact `Qiuyixuan_last@outlook.com` if required."
    #elif TARGET_OS_IPHONE
        #error "Not supported. Please contact `Qiuyixuan_last@outlook.com` if required."
    #elif TARGET_OS_MAC
        #define OS_NAME "osx"
    #else
        #error "Not supported. Please contact `Qiuyixuan_last@outlook.com` if required."
    #endif
#elif __linux__
    #define OS_NAME "linux"
#elif __unix__
    #error "Not supported. Please contact `Qiuyixuan_last@outlook.com` if required."
#elif defined(_POSIX_VERSION)
    #error "Not supported. Please contact `Qiuyixuan_last@outlook.com` if required."
#else
    #error "Not supported. Please contact `Qiuyixuan_last@outlook.com` if required."
#endif