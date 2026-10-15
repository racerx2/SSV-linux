/****************************************************************************
  This file is part of Space Shuttle Vessel

  Crew controller positions for the flight recorder


  Space Shuttle Vessel is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  Space Shuttle Vessel is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Space Shuttle Vessel; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  See https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html or
  file SSV-LICENSE.txt for more details.

  **************************************************************************/
/******* SSV File Modification Notice *******
Date         Developer
2026/10/15   racerx2
********************************************/
// not upstream (Orbiter Linux): flight recorder, GLS-SSV/SSV issue #28 (design ssv-flightrec)
#include "CrewControls.h"
#include <charconv>
#include <cmath>
#include <cstring>


namespace
{
	// numbers with '.' whatever the locale
	void Put( std::string& s, double v )
	{
		char cbuf[32];
		auto r = std::to_chars( cbuf, cbuf + sizeof(cbuf), v, std::chars_format::general, 4 );
		if (!s.empty()) s += ' ';
		s.append( cbuf, r.ptr );
	}

	void Put( std::string& s, int v )
	{
		char cbuf[16];
		auto r = std::to_chars( cbuf, cbuf + sizeof(cbuf), v );
		if (!s.empty()) s += ' ';
		s.append( cbuf, r.ptr );
	}

	bool Get( const char*& p, double& v, double lo, double hi )
	{
		while ((*p == ' ') || (*p == '\t')) p++;
		const char* e = p + strlen( p );
		auto r = std::from_chars( p, e, v );
		if ((r.ec != std::errc()) || !(v >= lo) || !(v <= hi)) return false;
		p = r.ptr;
		return true;
	}

	bool Get( const char*& p, int& v, int lo, int hi )
	{
		while ((*p == ' ') || (*p == '\t')) p++;
		const char* e = p + strlen( p );
		auto r = std::from_chars( p, e, v );
		if ((r.ec != std::errc()) || (v < lo) || (v > hi)) return false;
		p = r.ptr;
		return true;
	}

	bool Moved( double last, double now )
	{
		if (last == now) return false;
		if (std::fabs( now - last ) > 0.005) return true;
		return (now == 0.0) || (std::fabs( now ) == 1.0);
	}
}

std::string FormatCrewControls( const CrewControls& c )
{
	std::string s;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) Put( s, c.rhc[i][j] );
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 3; j++) Put( s, static_cast<int>(c.thc[i][j]) );
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++) Put( s, static_cast<int>(c.trim[i][j]) );
	Put( s, c.rpta );
	for (int i = 0; i < 2; i++)
	{
		Put( s, c.sbtc[i] );
		Put( s, c.sbtcto[i] ? 1 : 0 );
	}
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++) Put( s, c.brake[i][j] );
	for (int i = 0; i < 3; i++) Put( s, c.rmsrhc[i] );
	for (int i = 0; i < 3; i++) Put( s, c.rmsthc[i] );
	Put( s, static_cast<int>(c.rmssj) );
	return s;
}

bool ParseCrewControls( const char* s, CrewControls& c )
{
	if (!s) return false;
	CrewControls t;
	const char* p = s;
	int n;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) if (!Get( p, t.rhc[i][j], -1.0, 1.0 )) return false;
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 3; j++)
		{
			if (!Get( p, n, -1, 1 )) return false;
			t.thc[i][j] = static_cast<short>(n);
		}
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
		{
			if (!Get( p, n, -1, 1 )) return false;
			t.trim[i][j] = static_cast<short>(n);
		}
	if (!Get( p, t.rpta, -1.0, 1.0 )) return false;
	for (int i = 0; i < 2; i++)
	{
		if (!Get( p, t.sbtc[i], 0.0, 1.0 )) return false;
		if (!Get( p, n, 0, 1 )) return false;
		t.sbtcto[i] = (n != 0);
	}
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++) if (!Get( p, t.brake[i][j], 0.0, 1.0 )) return false;
	for (int i = 0; i < 3; i++) if (!Get( p, t.rmsrhc[i], -1.0, 1.0 )) return false;
	for (int i = 0; i < 3; i++) if (!Get( p, t.rmsthc[i], -1.0, 1.0 )) return false;
	if (!Get( p, n, -1, 1 )) return false;
	t.rmssj = static_cast<short>(n);
	c = t;
	return true;
}

bool CrewControlsDiffer( const CrewControls& last, const CrewControls& now )
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) if (Moved( last.rhc[i][j], now.rhc[i][j] )) return true;
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 3; j++) if (last.thc[i][j] != now.thc[i][j]) return true;
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++) if (last.trim[i][j] != now.trim[i][j]) return true;
	if (Moved( last.rpta, now.rpta )) return true;
	for (int i = 0; i < 2; i++)
	{
		if (Moved( last.sbtc[i], now.sbtc[i] )) return true;
		if (last.sbtcto[i] != now.sbtcto[i]) return true;
	}
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++) if (Moved( last.brake[i][j], now.brake[i][j] )) return true;
	for (int i = 0; i < 3; i++)
	{
		if (Moved( last.rmsrhc[i], now.rmsrhc[i] )) return true;
		if (Moved( last.rmsthc[i], now.rmsthc[i] )) return true;
	}
	return last.rmssj != now.rmssj;
}
