/****************************************************************************
  Space Shuttle Vessel - Linux port, modified by racerx2, 2026

  Space Shuttle Vessel is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  See https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html or
  file SSV-LICENSE.txt for more details.
  **************************************************************************/
// not upstream: the MSVC "secure" C library functions SSV calls (ssv_crt.h)
#include "ssv_crt.h"
#include <cerrno>
#include <climits>
#include <string>

int vsprintf_s (char *buf, size_t size, const char *fmt, va_list ap)
{
	if (!buf || !size) return -1;
	if (!fmt) { buf[0] = '\0'; return -1; }
	int n = vsnprintf (buf, size, fmt, ap);
	if (n < 0 || size_t (n) >= size) { buf[0] = '\0'; return -1; } // too long or an encoding error
	return n;
}

int sprintf_s (char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	int n = vsprintf_s (buf, size, fmt, ap);
	va_end (ap);
	return n;
}

namespace {

// one directive of a scanf format, with the literal text before it
struct Directive {
	std::string literal;  // text to match first, '%%' kept
	bool suppress = false;
	int width = -1;       // -1: none given
	std::string length;   // h, hh, l, ll, L, j, z, t
	std::string set;      // the scanset of %[, brackets included
	char conv = 0;        // 0: only a trailing literal
};

// splits fmt at its next directive; returns the position after it
const char *NextDirective (const char *f, Directive &d)
{
	d = Directive ();
	while (*f) {
		if (f[0] == '%' && f[1] == '%') { d.literal += "%%"; f += 2; continue; }
		if (f[0] != '%') { d.literal += *f++; continue; }
		f++;
		if (*f == '*') { d.suppress = true; f++; }
		if (*f >= '0' && *f <= '9') { d.width = 0; while (*f >= '0' && *f <= '9') d.width = d.width * 10 + (*f++ - '0'); }
		while (*f && strchr ("hlLjzt", *f)) d.length += *f++;
		if (!*f) return f;
		d.conv = *f++;
		if (d.conv == '[') { // the set runs to the first ']' that is not its first member
			d.set = "[";
			if (*f == '^') d.set += *f++;
			if (*f == ']') d.set += *f++;
			while (*f && *f != ']') d.set += *f++;
			if (*f == ']') d.set += *f++;
		}
		return f;
	}
	return f;
}

} // namespace

int vsscanf_s (const char *str, const char *fmt, va_list ap)
{
	if (!str || !fmt) return EOF;
	const char *s = str;
	int assigned = 0;
	Directive d;
	const char *f = fmt;
	while (*f) {
		f = NextDirective (f, d);
		int n = -1;
		if (!d.conv) { // trailing literal: its match does not change the count
			std::string piece = d.literal + "%n";
			sscanf (s, piece.c_str (), &n);
			break;
		}
		std::string spec = std::string (d.suppress ? "%*" : "%");
		if (d.conv == 'n') { // characters read so far, from the start of str
			std::string piece = d.literal + "%n";
			int r = sscanf (s, piece.c_str (), &n);
			if (n < 0) return (r == EOF && !assigned) ? EOF : assigned;
			s += n;
			if (!d.suppress) {
				void *p = va_arg (ap, void *);
				long pos = long (s - str);
				if (d.length == "hh") *(signed char *)p = (signed char)pos;
				else if (d.length == "h") *(short *)p = (short)pos;
				else if (d.length == "l") *(long *)p = pos;
				else if (d.length == "ll") *(long long *)p = pos;
				else *(int *)p = int (pos);
			}
			continue;
		}
		bool sized = d.conv == 's' || d.conv == 'c' || d.conv == '[';
		void *p = d.suppress ? nullptr : va_arg (ap, void *);
		unsigned size = (sized && !d.suppress) ? va_arg (ap, unsigned) : 0;
		if (sized && !d.suppress && d.length.empty ()) {
			if (!size) return assigned; // MSVC: a zero size fails the conversion
			int r;
			if (d.conv == 'c') { // count characters, no NUL
				int count = d.width < 0 ? 1 : d.width;
				if (unsigned (count) > size) { ((char *)p)[0] = '\0'; return assigned; }
				std::string piece = d.literal + "%" + std::to_string (count) + "c%n";
				r = sscanf (s, piece.c_str (), (char *)p, &n);
			} else { // %s, %[: one character more than fits, so a longer field is seen and fails
				unsigned room = size - 1;
				unsigned w = (d.width >= 0 && unsigned (d.width) <= room) ? unsigned (d.width) : room + 1;
				std::string tmp (size_t (w) + 1, '\0');
				std::string piece = d.literal + "%" + std::to_string (w) + (d.conv == 's' ? std::string ("s") : d.set) + "%n";
				r = sscanf (s, piece.c_str (), &tmp[0], &n);
				if (r == 1 && n >= 0) {
					size_t len = strlen (tmp.c_str ());
					if (len > room) { ((char *)p)[0] = '\0'; return assigned; }
					memcpy (p, tmp.c_str (), len + 1);
				}
			}
			if (r == EOF) return assigned ? assigned : EOF; // as glibc: EOF until something is assigned
			if (r != 1 || n < 0) return assigned;
			s += n;
			assigned++;
			continue;
		}
		// numbers, pointers and the suppressed fields: sscanf takes the directive as written
		spec += d.width >= 0 ? std::to_string (d.width) : std::string ();
		spec += d.length;
		spec += d.conv == '[' ? d.set : std::string (1, d.conv);
		std::string piece = d.literal + spec + "%n";
		int r = d.suppress ? sscanf (s, piece.c_str (), &n) : sscanf (s, piece.c_str (), p, &n);
		if (r == EOF) return assigned ? assigned : EOF;
		if (n < 0) return assigned; // the literal or the field did not match
		s += n;
		if (!d.suppress) assigned++;
	}
	return assigned;
}

int sscanf_s (const char *str, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	int r = vsscanf_s (str, fmt, ap);
	va_end (ap);
	return r;
}

errno_t strcpy_s (char *dst, size_t size, const char *src)
{
	if (!dst || !size) return EINVAL;
	if (!src) { dst[0] = '\0'; return EINVAL; }
	size_t n = strlen (src);
	if (n >= size) { dst[0] = '\0'; return ERANGE; }
	memcpy (dst, src, n + 1);
	return 0;
}

errno_t strcat_s (char *dst, size_t size, const char *src)
{
	if (!dst || !size) return EINVAL;
	if (!src) { dst[0] = '\0'; return EINVAL; }
	size_t d = strnlen (dst, size);
	if (d == size) { dst[0] = '\0'; return EINVAL; } // not terminated
	size_t n = strlen (src);
	if (d + n >= size) { dst[0] = '\0'; return ERANGE; }
	memcpy (dst + d, src, n + 1);
	return 0;
}

errno_t strncpy_s (char *dst, size_t size, const char *src, size_t count)
{
	if (!dst || !size) return EINVAL;
	if (!src) { dst[0] = '\0'; return EINVAL; }
	size_t n = strnlen (src, count);
	if (n >= size) { dst[0] = '\0'; return ERANGE; }
	memcpy (dst, src, n);
	dst[n] = '\0';
	return 0;
}

errno_t fopen_s (FILE **f, const char *name, const char *mode)
{
	if (!f) return EINVAL;
	*f = fopen (name, mode);
	return *f ? 0 : errno;
}
