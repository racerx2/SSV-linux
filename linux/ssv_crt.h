/****************************************************************************
  Space Shuttle Vessel - Linux port, modified by racerx2, 2026

  Space Shuttle Vessel is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  See https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html or
  file SSV-LICENSE.txt for more details.
  **************************************************************************/
// not upstream: the MSVC "secure" C library functions SSV calls (C11 Annex K), with MSVC's results; force-included by the Linux build
#ifndef __SSV_CRT_H
#define __SSV_CRT_H

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <strings.h>

typedef int errno_t;

// formatted output: the whole result with its NUL fits, else an empty string and -1 (MSVC's invalid parameter path, without the abort)
int vsprintf_s (char *buf, size_t size, const char *fmt, va_list ap);
int sprintf_s (char *buf, size_t size, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
template<size_t N> int sprintf_s (char (&buf)[N], const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
template<size_t N> int sprintf_s (char (&buf)[N], const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	int n = vsprintf_s (buf, N, fmt, ap);
	va_end (ap);
	return n;
}

// formatted input: %s, %c and %[ take a size argument after their pointer; a field that does not fit fails, as MSVC's does
int vsscanf_s (const char *str, const char *fmt, va_list ap);
int sscanf_s (const char *str, const char *fmt, ...);

// string copies: 0, or an error with an empty destination
errno_t strcpy_s (char *dst, size_t size, const char *src);
errno_t strcat_s (char *dst, size_t size, const char *src);
errno_t strncpy_s (char *dst, size_t size, const char *src, size_t count);
template<size_t N> errno_t strcpy_s (char (&dst)[N], const char *src) { return strcpy_s (dst, N, src); }
template<size_t N> errno_t strcat_s (char (&dst)[N], const char *src) { return strcat_s (dst, N, src); }
template<size_t N> errno_t strncpy_s (char (&dst)[N], const char *src, size_t count) { return strncpy_s (dst, N, src, count); }

errno_t fopen_s (FILE **f, const char *name, const char *mode);

inline int _stricmp (const char *a, const char *b) { return strcasecmp (a, b); }
inline int _strnicmp (const char *a, const char *b, size_t n) { return strncasecmp (a, b, n); }

#endif // __SSV_CRT_H
