/******* SSV File Modification Notice *******
Date         Developer
2020/06/20   GLS
2021/08/24   GLS
2021/10/23   GLS
2022/08/05   GLS
2022/09/30   GLS
2026/10/15   racerx2
********************************************/
#include <Stopwatch.h>

Stopwatch::Stopwatch()
{
#ifdef _WIN32
	QueryPerformanceFrequency(&liFrequency);
#endif
}

Stopwatch::~Stopwatch()
{
}

void Stopwatch::Start()
{
#ifdef _WIN32
	QueryPerformanceCounter(&liLast);
#else
	tLast = std::chrono::steady_clock::now();// not upstream
#endif
}

double Stopwatch::Stop()
{
#ifdef _WIN32
	QueryPerformanceCounter(&liCurrent);

	double fPerfElapsed =(((liCurrent.QuadPart - liLast.QuadPart)
            * 1000000.0) / liFrequency.QuadPart);
#else
	double fPerfElapsed = std::chrono::duration<double, std::micro>( std::chrono::steady_clock::now() - tLast ).count();// not upstream: microseconds, as above
#endif

	return fPerfElapsed;
}
