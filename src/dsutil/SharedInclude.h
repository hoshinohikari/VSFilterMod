#ifndef IncDSUtil_SharedInclude_h
#define IncDSUtil_SharedInclude_h

// Ensure C++ stdint/cstdint headers expose integer limit macros like PTRDIFF_MAX.
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stdint.h>

#ifndef PTRDIFF_MAX
#ifdef _WIN64
#define PTRDIFF_MAX _I64_MAX
#else
#define PTRDIFF_MAX _I32_MAX
#endif
#endif

#pragma warning(disable:4018)
#pragma warning(disable:4800)
#pragma warning(disable:4355)
#pragma warning(disable:4244)
#pragma warning(disable:4995)
#pragma warning(disable:4996)
#pragma warning(disable:4305)
#pragma warning(disable:4200)
#pragma warning(disable:4101)
#pragma warning(disable:4101)
#pragma warning(disable:4267)

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC // include Microsoft memory leak detection procedures

#if 0
#include <crtdbg.h>
#define DNew new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define DNew new(__FILE__, __LINE__)
#endif

#else

#define DNew new

#endif
#endif // IncDSUtil_SharedInclude_h


