#ifndef DB_EXPORT_HPP
#define DB_EXPORT_HPP

/**
 * @file DbExport.hpp
 * @brief DLL/SO export macros for DbLib
 */

#if defined(PULSEONE_STATIC) && !defined(DBLIB_STATIC)
#define DBLIB_STATIC
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef DBLIB_STATIC
#define DBLIB_API
#elif defined(DBLIB_EXPORTS)
#define DBLIB_API __declspec(dllexport)
#else
#define DBLIB_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define DBLIB_API __attribute__((visibility("default")))
#else
#define DBLIB_API
#endif
#endif

#endif // DB_EXPORT_HPP
