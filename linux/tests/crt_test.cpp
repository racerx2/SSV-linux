/****************************************************************************
  Space Shuttle Vessel - Linux port, modified by racerx2, 2026
  Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt)
  **************************************************************************/
// not upstream: tests of ssv_crt (the MSVC secure C library functions), against glibc where the results must agree
#include "ssv_crt.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; fprintf (stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void Printf ()
{
	char b[8];
	CHECK (sprintf_s (b, "%d", 1234567) == 7 && !strcmp (b, "1234567"));
	CHECK (sprintf_s (b, "%d", 12345678) == -1 && b[0] == '\0'); // 8 characters and the NUL do not fit
	char big[64];
	CHECK (sprintf_s (big, sizeof (big), "%s-%04.1f", "x", 2.25) == 6 && !strcmp (big, "x-02.2"));
	CHECK (sprintf_s (big, 4, "%s", "abc") == 3 && !strcmp (big, "abc"));
	CHECK (sprintf_s (big, 3, "%s", "abc") == -1 && big[0] == '\0');
	CHECK (sprintf_s (nullptr, 4, "x") == -1);
	// the invalid parameter path: errno set, reported on stderr
	errno = 0; CHECK (sprintf_s (b, "%d", 12345678) == -1 && errno == ERANGE);
	errno = 0; CHECK (sprintf_s (big, sizeof (big), nullptr) == -1 && big[0] == '\0' && errno == EINVAL);
}

static void Strings ()
{
	char b[4];
	CHECK (strcpy_s (b, "abc") == 0 && !strcmp (b, "abc"));
	CHECK (strcpy_s (b, "abcd") != 0 && b[0] == '\0');
	CHECK (strcpy_s (b, "ab") == 0 && strcat_s (b, "c") == 0 && !strcmp (b, "abc"));
	CHECK (strcat_s (b, "d") != 0 && b[0] == '\0');
	CHECK (strncpy_s (b, "abcdef", 3) == 0 && !strcmp (b, "abc"));
	CHECK (strncpy_s (b, "abcdef", 4) != 0 && b[0] == '\0');
	CHECK (_stricmp ("SSV_OV", "ssv_ov") == 0 && _strnicmp ("ABCx", "abcy", 3) == 0 && _strnicmp ("ABCx", "abcy", 4) != 0);
	errno = 0; CHECK (strcpy_s (b, "abcd") == ERANGE && errno == ERANGE);
	errno = 0; CHECK (strcpy_s (b, nullptr) == EINVAL && b[0] == '\0' && errno == EINVAL);
	errno = 0; CHECK (strcpy_s (nullptr, 4, "a") == EINVAL && errno == EINVAL);
	CHECK (strcpy_s (b, "ab") == 0);
	errno = 0; CHECK (strcat_s (b, "cd") == ERANGE && b[0] == '\0' && errno == ERANGE);
	errno = 0; CHECK (strncpy_s (b, "abcdef", 4) == ERANGE && errno == ERANGE);
	errno = 0; CHECK (strncpy_s (b, nullptr, 1) == EINVAL && errno == EINVAL);
	FILE *f = nullptr;
	CHECK (fopen_s (&f, "/nonexistent/file", "r") != 0 && f == nullptr);
	f = stdout; CHECK (fopen_s (&f, nullptr, "r") == EINVAL && f == nullptr);
	f = stdout; CHECK (fopen_s (&f, "/dev/null", nullptr) == EINVAL && f == nullptr);
	CHECK (fopen_s (nullptr, "/dev/null", "r") == EINVAL);
}

// every format SSV passes to sscanf_s without a size argument agrees with glibc sscanf
static void ScanNumbers ()
{
	{ double a = 0, b = 0, c = 0; CHECK (sscanf_s ("1.5 -2e3 7", "%lf %lf %lf", &a, &b, &c) == 3 && a == 1.5 && b == -2000.0 && c == 7.0); }
	{ double a = 0, b = 0, c = 0, d = 0; CHECK (sscanf_s ("1 2 3 4", "%lf%lf%lf%lf", &a, &b, &c, &d) == 4 && d == 4.0); }
	{ int a = 0, b = 0, c = 0; CHECK (sscanf_s ("4 -5 6", "%d %d %d", &a, &b, &c) == 3 && a == 4 && b == -5 && c == 6); }
	{ unsigned a = 0, b = 0, c = 0; CHECK (sscanf_s ("7 8 9", "%u %u %u", &a, &b, &c) == 3 && c == 9); }
	{ unsigned short a = 0; double b = 0; CHECK (sscanf_s ("12 3.25", "%hu %lf", &a, &b) == 2 && a == 12 && b == 3.25); }
	{ short a = 0, c = 0, d = 0; double b = 0; CHECK (sscanf_s ("-1 2.5 3 4", "%hd %lf %hd %hd", &a, &b, &c, &d) == 4 && a == -1 && d == 4); }
	{ unsigned char x = 0; CHECK (sscanf_s ("af", "%02hhx", &x) == 1 && x == 0xaf); }
	{ unsigned char x = 0; CHECK (sscanf_s ("1f2", "%02hhx", &x) == 1 && x == 0x1f); }
	{ int i = 0; CHECK (sscanf_s ("0x10", "%i", &i) == 1 && i == 16); }
	{ float f = 0; CHECK (sscanf_s ("2.5", "%f", &f) == 1 && f == 2.5f); }
	{ int a = 0; double b = 0; CHECK (sscanf_s ("3 4.5", "%d%lf", &a, &b) == 2 && b == 4.5); }
	// failures: none matched, part matched, empty input
	{ int a = 9; CHECK (sscanf_s ("x", "%d", &a) == 0 && a == 9); }
	{ int a = 0, b = 9; CHECK (sscanf_s ("1 x", "%d %d", &a, &b) == 1 && a == 1 && b == 9); }
	{ int a = 0; CHECK (sscanf_s ("", "%d", &a) == EOF); }
	{ int a = 0; CHECK (sscanf_s ("   ", "%d", &a) == EOF); }
	{ int a = 0, b = 0; CHECK (sscanf_s ("5", "%d %d", &a, &b) == 1); }
	// literals, %% and suppression
	{ int a = 0, b = 0; CHECK (sscanf_s ("1,2", "%d,%d", &a, &b) == 2 && b == 2); }
	{ int a = 0, b = 0; CHECK (sscanf_s ("1;2", "%d,%d", &a, &b) == 1); }
	{ int a = 0; CHECK (sscanf_s ("50%", "%d%%", &a) == 1 && a == 50); }
	{ int a = 0; CHECK (sscanf_s ("1 2", "%*d %d", &a) == 1 && a == 2); }
	{ int a = 0, n = 0; CHECK (sscanf_s ("12 ab", "%d %n", &a, &n) == 1 && n == 3); }
}

// %s, %c and %[ with their size arguments
static void ScanStrings ()
{
	{ char s[8]; CHECK (sscanf_s ("SSV_OV rest", "%s", s, (unsigned)sizeof (s)) == 1 && !strcmp (s, "SSV_OV")); }
	{ char s[4]; CHECK (sscanf_s ("abcd", "%s", s, (unsigned)sizeof (s)) == 0 && s[0] == '\0'); } // MSVC: too long fails
	{ char s[5]; CHECK (sscanf_s ("abcd", "%s", s, (unsigned)sizeof (s)) == 1 && !strcmp (s, "abcd")); }
	{ char s[8]; CHECK (sscanf_s ("abcdef", "%3s", s, (unsigned)sizeof (s)) == 1 && !strcmp (s, "abc")); }
	{ char a[16], b[16]; int n = 0;
	  CHECK (sscanf_s ("7,KSC 15,SLF", "%d,%[^,],%[^,]", &n, a, (unsigned)sizeof (a), b, (unsigned)sizeof (b)) == 3 && n == 7 && !strcmp (a, "KSC 15") && !strcmp (b, "SLF")); }
	{ char a[16], b[16], c[16]; double x = 0, y = 0, z = 0, w = 0; unsigned short h = 0;
	  CHECK (sscanf_s ("EDW,22,Edwards,34.9,-117.8,700,45.5,3", "%[^,],%[^,],%[^,],%lf,%lf,%lf,%lf,%hu",
		a, (unsigned)sizeof (a), b, (unsigned)sizeof (b), c, (unsigned)sizeof (c), &x, &y, &z, &w, &h) == 8 && !strcmp (c, "Edwards") && h == 3 && w == 45.5); }
	{ char s[3]; CHECK (sscanf_s ("xyz", "%[^,]", s, (unsigned)sizeof (s)) == 0 && s[0] == '\0'); }
	{ char c[2]; CHECK (sscanf_s ("ab", "%2c", c, 2u) == 1 && c[0] == 'a' && c[1] == 'b'); }
	{ char c[2]; CHECK (sscanf_s ("abc", "%3c", c, 2u) == 0); }
	{ char c = 0; int a = 0; CHECK (sscanf_s ("q 5", "%c %d", &c, 1u, &a) == 2 && c == 'q' && a == 5); }
	{ char s[8]; int a = 0; CHECK (sscanf_s ("abc 9", "%s %d", s, 8u, &a) == 2 && a == 9); }
	{ char s[8]; CHECK (sscanf_s ("", "%s", s, 8u) == EOF); }
	{ char s[8]; CHECK (sscanf_s ("ab", "%*s %s", s, 8u) == EOF); }
	{ int a = 0; CHECK (sscanf_s ("5", "%*d %d", &a) == EOF); } // glibc: EOF until something is assigned
	// a bare sizeof (size_t) as SSV passes it (LCC.cpp:762): the size is read as unsigned, as MSVC reads it
	{ char s[8]; CHECK (sscanf_s ("LC39A rest", "%s", s, sizeof (s)) == 1 && !strcmp (s, "LC39A")); }
	{ char s[4]; CHECK (sscanf_s ("LC39A", "%s", s, sizeof (s)) == 0 && s[0] == '\0'); }
	{ char a[8]; int n = 0; CHECK (sscanf_s ("SLF,3", "%[^,],%d", a, sizeof (a), &n) == 2 && !strcmp (a, "SLF") && n == 3); }
	{ char c = 0; int a = 0; CHECK (sscanf_s ("q 5", "%c %d", &c, sizeof (c), &a) == 2 && c == 'q' && a == 5); }
	errno = 0; CHECK (sscanf_s (nullptr, "%d", nullptr) == EOF && errno == EINVAL);
}

int main ()
{
	Printf ();
	Strings ();
	ScanNumbers ();
	ScanStrings ();
	printf ("%d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}
