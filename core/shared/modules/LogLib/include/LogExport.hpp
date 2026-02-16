#ifndef LOG_EXPORT_HPP
#define LOG_EXPORT_HPP

/**
 * @file LogExport.hpp
 * @brief DLL/SO export macros for LogLib
 */

#if defined(PULSEONE_STATIC) && !defined(LOGLIB_STATIC)
#define LOGLIB_STATIC
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef LOGLIB_STATIC
#define LOGLIB_API
#elif defined(LOGLIB_EXPORTS)
#define LOGLIB_API __declspec(dllexport)
#else
#define LOGLIB_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define LOGLIB_API __attribute__((visibility("default")))
#else
#define LOGLIB_API
#endif
#endif

#endif // LOG_EXPORT_HPP
