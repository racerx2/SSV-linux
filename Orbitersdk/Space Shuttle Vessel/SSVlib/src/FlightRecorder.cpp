/****************************************************************************
  This file is part of Space Shuttle Vessel

  Flight recorder support


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
// not upstream (Orbiter Linux): Orbiter flight recorder support, GLS-SSV/SSV issue #28 (design ssv-flightrec)
#include <FlightRecorder.h>
#include <cstdio>
#include <set>


namespace flightrec
{
	// vessels of this module playing back nominal
	static int nominal = 0;

	double Rand( void )
	{
		return nominal ? 0.5 : oapiRand();
	}

	double Held( double u )
	{
		return nominal ? 0.5 : u;
	}

	bool Nominal( void )
	{
		return nominal != 0;
	}

	std::string Quote( const std::string& s )
	{
		return "\"" + s + "\"";
	}

	const char* SkipBlanks( const char* p )
	{
		while ((*p == ' ') || (*p == '\t')) p++;
		return p;
	}

	bool NextToken( const char*& p, std::string& out )
	{
		const char* s = SkipBlanks( p );
		if (*s == '\0') return false;
		if (*s == '\"')
		{
			const char* e = s + 1;
			while ((*e != '\0') && (*e != '\"')) e++;
			if (*e != '\"') return false;
			out.assign( s + 1, e );
			p = e + 1;
			return true;
		}
		const char* e = s;
		while ((*e != '\0') && (*e != ' ') && (*e != '\t')) e++;
		out.assign( s, e );
		p = e;
		return true;
	}

	std::string OneLine( const std::string& s )
	{
		std::string r = s;
		for (char& c : r)
		{
			if ((static_cast<unsigned char>(c) < 32) || (c == 127)) c = ' ';
		}
		return r;
	}

	void LogUnknown( const char* module, const char* type, const std::string& key )
	{
		static std::set<std::string> logged;
		if (logged.insert( std::string( type ) + " " + key ).second)
			oapiWriteLogV( "(%s) [WARNING] Flight recorder: nothing plays back %s %s", module, type, key.c_str() );
		return;
	}


	Recorder::Recorder( VESSEL* v ):v(v), steps(0), started(false), locked(false)
	{
	}

	Recorder::~Recorder( void )
	{
		if (locked) nominal--;
	}

	void Recorder::Step( void )
	{
		const int prev = steps;
		steps = v->Recording() ? ((steps < 2) ? (steps + 1) : 2) : 0;
		started = (steps == 2) && (prev == 1);
		if (started)
		{
			char cbuf[16];
			snprintf( cbuf, sizeof cbuf, "%d", FORMAT );
			v->RecordEvent( NOMINAL, cbuf );
		}

		if (locked && !v->Playback())
		{
			locked = false;
			nominal--;
			oapiWriteLogV( "(SSV) [INFO] Flight recorder: %s playback ended", v->GetName() );
		}
		return;
	}

	bool Recorder::OnPlaybackEvent( const char* type, const char* event )
	{
		if (!_stricmp( type, NOMINAL ))
		{
			if (!locked && v->Playback())
			{
				locked = true;
				nominal++;
				oapiWriteLogV( "(SSV) [INFO] Flight recorder: %s plays back nominal (format %s)", v->GetName(), SkipBlanks( event ? event : "" ) );
			}
			return true;
		}
		return !_stricmp( type, EVENT ) || !_stricmp( type, CW ) || !_stricmp( type, MA );
	}

	bool Recorder::Recording( void ) const
	{
		return v->Recording();
	}

	void Recorder::Record( const char* type, const std::string& text ) const
	{
		if (v->Recording()) v->RecordEvent( type, OneLine( text ).c_str() );
		return;
	}
}
