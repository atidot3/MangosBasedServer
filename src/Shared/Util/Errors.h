#ifndef ERRORS_H
#define ERRORS_H

#include "../Common.h"

// Normal assert.
#define WPError(CONDITION) \
if (!(CONDITION)) \
{ \
    printf("%s:%i: Error: Assertion in %s failed: %s", \
        __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION)); \
    assert(STRINGIZE(CONDITION) && 0); \
}

// Just warn.
#define WPWarning(CONDITION) \
if (!(CONDITION)) \
{ \
    printf("%s:%i: Warning: Assertion in %s failed: %s",\
        __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION)); \
}

#ifdef ORIGIN_DEBUG
#  define ORIGIN_ASSERT WPError
#else
#  define ORIGIN_ASSERT WPError                             // Error even if in release mode.
#endif

#endif
