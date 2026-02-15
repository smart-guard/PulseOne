#ifndef PULSEONE_PLATFORM_API_H
#define PULSEONE_PLATFORM_API_H

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#ifdef PULSEONE_EXPORT
#define PULSEONE_API __declspec(dllexport)
#else
#ifdef PULSEONE_STATIC
#define PULSEONE_API
#else
#define PULSEONE_API __declspec(dllimport)
#endif
#endif
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define PULSEONE_API __attribute__((visibility("default")))
#else
#define PULSEONE_API
#endif
#endif

#endif // PULSEONE_PLATFORM_API_H
