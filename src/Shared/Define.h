#ifndef DEFINE_H
#define DEFINE_H

#include "CompilerDefs.h"

#include <cstdint>

#include <sys/types.h>

#define ORIGIN_LITTLEENDIAN 0
#define ORIGIN_BIGENDIAN    1

#if !defined(ORIGIN_ENDIAN)
#  if defined (ACE_BIG_ENDIAN)
#    define ORIGIN_ENDIAN ORIGIN_BIGENDIAN
#  else // ACE_BYTE_ORDER != ACE_BIG_ENDIAN
#    define ORIGIN_ENDIAN ORIGIN_LITTLEENDIAN
#  endif // ACE_BYTE_ORDER
#endif // ORIGIN_ENDIAN

#define ORIGIN_SCRIPT_NAME "ORIGINscript"
#define ORIGIN_PATH_MAX 1024

#if PLATFORM == PLATFORM_WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0603
#  endif
typedef HMODULE ORIGIN_LIBRARY_HANDLE;
#  define ORIGIN_SCRIPT_SUFFIX ".dll"
#  define ORIGIN_SCRIPT_PREFIX ""
#  define ORIGIN_LOAD_LIBRARY(libname)     LoadLibraryA(libname)
#  define ORIGIN_CLOSE_LIBRARY(hlib)       FreeLibrary(hlib)
#  define ORIGIN_GET_PROC_ADDR(hlib, name) GetProcAddress(hlib, name)
#  define ORIGIN_EXPORT __declspec(dllexport)
#  define ORIGIN_IMPORT __cdecl
#else // PLATFORM != PLATFORM_WINDOWS
#  include <dlfcn.h>
typedef void* ORIGIN_LIBRARY_HANDLE;
#  define ORIGIN_LOAD_LIBRARY(libname)     dlopen(libname, RTLD_LAZY)
#  define ORIGIN_CLOSE_LIBRARY(hlib)       dlclose(hlib)
#  define ORIGIN_GET_PROC_ADDR(hlib, name) dlsym(hlib, name)
#  define ORIGIN_EXPORT export
#  if PLATFORM == PLATFORM_APPLE
#    define ORIGIN_SCRIPT_SUFFIX ".dylib"
#  else
#    define ORIGIN_SCRIPT_SUFFIX ".so"
#  endif
#  define ORIGIN_SCRIPT_PREFIX "lib"
#  if defined(__APPLE_CC__) && defined(BIG_ENDIAN) // TODO:: more work to do with byte order. Have to be rechecked after boost integration.
#    if (defined (__ppc__) || defined (__powerpc__))
#      define ORIGIN_IMPORT __attribute__ ((longcall))
#    else
#      define ORIGIN_IMPORT
#    endif
#  elif defined(__x86_64__)
#    define ORIGIN_IMPORT
#  else
#    define ORIGIN_IMPORT __attribute__ ((cdecl))
#  endif //__APPLE_CC__ && BIG_ENDIAN
#endif // PLATFORM

#if PLATFORM == PLATFORM_WINDOWS
#  ifndef DECLSPEC_NORETURN
#    define DECLSPEC_NORETURN __declspec(noreturn)
#  endif // DECLSPEC_NORETURN
#else // PLATFORM != PLATFORM_WINDOWS
#  define DECLSPEC_NORETURN
#endif // PLATFORM

#if !defined(DEBUG)
#  define ORIGIN_INLINE inline
#else // DEBUG
#  if !defined(ORIGIN_DEBUG)
#    define ORIGIN_DEBUG
#  endif // ORIGIN_DEBUG
#  define ORIGIN_INLINE
#endif //!DEBUG

#if COMPILER == COMPILER_GNU
#  define ATTR_NORETURN __attribute__((noreturn))
#  define ATTR_PRINTF(F,V) __attribute__ ((format (printf, F, V)))
#else // COMPILER != COMPILER_GNU
#  define ATTR_NORETURN
#  define ATTR_PRINTF(F,V)
#endif // COMPILER == COMPILER_GNU

typedef std::int64_t int64;
typedef std::int32_t int32;
typedef std::int16_t int16;
typedef std::int8_t int8;
typedef std::uint64_t uint64;
typedef std::uint32_t uint32;
typedef std::uint16_t uint16;
typedef std::uint8_t uint8;

#if COMPILER == COMPILER_GNU
#  if !defined(__GXX_EXPERIMENTAL_CXX0X__) || (__GNUC__ < 4) || (__GNUC__ == 4) && (__GNUC_MINOR__ < 7)
#    define override
#  endif
#endif

typedef uint64 OBJECT_HANDLE;

#endif // ORIGIN_DEFINE_H