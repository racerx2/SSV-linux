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
#include <cstdlib>
#include <cctype>
#include <set>
#include <vector>
#include <algorithm>


namespace
{
	// this module's kept draws, live ones in construction order; never freed, so no exit order (design ssv-flightrec rev 4)
	std::vector<flightrec::Draw*>& Slots( void )
	{
		static std::vector<flightrec::Draw*>* slots = new std::vector<flightrec::Draw*>;
		return *slots;
	}

	// LIVE: no playback; WAIT: until the recording's draws arrived; DRAWN: they did; NOMINAL: other draws or format 1
	enum class Mode { LIVE, WAIT, DRAWN, NOMINAL };
	Mode mode = Mode::LIVE;

	// recorders of this module playing back
	int playbacks = 0;

	constexpr size_t DRAWS_LINE = 16;// values in an SSV_DRAWS line, under Orbiter's 1024 characters
}


namespace flightrec
{
	// vessels of this module playing back nominal
	static int nominal = 0;

	double Rand( void )
	{
		return nominal ? 0.5 : oapiRand();
	}

	Draw::Draw( double u ):u(u)
	{
		Slots().push_back( this );
	}

	Draw::Draw( const Draw& d ):u(d.u)
	{
		Slots().push_back( this );
	}

	Draw& Draw::operator=( const Draw& d )
	{
		u = d.u;
		return *this;
	}

	Draw::~Draw( void )
	{
		std::vector<Draw*>& slots = Slots();
		const auto it = std::find( slots.begin(), slots.end(), this );
		if (it != slots.end()) slots.erase( it );
	}

	double Draw::Get( void ) const
	{
		return ((mode == Mode::WAIT) || (mode == Mode::NOMINAL)) ? 0.5 : u;
	}

	void Draw::Redraw( const Recorder& r )
	{
		if (r.PlayingBack()) return;
		u = oapiRand();
		if (r.Recording())
		{
			const std::vector<Draw*>& slots = Slots();
			const size_t i = std::find( slots.begin(), slots.end(), this ) - slots.begin();
			char cbuf[64];
			snprintf( cbuf, sizeof cbuf, "%zu %.17g", i, u );
			r.Record( DRAW, cbuf );
		}
		return;
	}

	std::string Quote( const std::string& s )
	{
		return "\"" + s + "\"";
	}

	const char* SkipBlanks( const char* p )
	{
		while (isspace( static_cast<unsigned char>(*p) )) p++;
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
		while ((*e != '\0') && !isspace( static_cast<unsigned char>(*e) )) e++;
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


	Recorder::Recorder( VESSEL* v ):v(v), steps(0), started(false), locked(false), playing(false)
	{
	}

	Recorder::~Recorder( void )
	{
		if (locked) nominal--;
		if (playing && (--playbacks == 0)) mode = Mode::LIVE;
	}

	void Recorder::Track( void )
	{
		const bool pb = v->Playback();
		if (pb == playing) return;
		playing = pb;
		if (pb)
		{
			if (playbacks++ == 0) mode = Mode::WAIT;
		}
		else if (--playbacks == 0) mode = Mode::LIVE;
		return;
	}

	void Recorder::Step( void )
	{
		const int prev = steps;
		steps = v->Recording() ? ((steps < 2) ? (steps + 1) : 2) : 0;
		started = (steps == 2) && (prev == 1);// marker on the second step: Orbiter truncates the .atc at its first save, stock Orbiter too
		if (started)
		{
			char cbuf[16];
			snprintf( cbuf, sizeof cbuf, "%d", FORMAT );
			v->RecordEvent( NOMINAL, cbuf );

			const std::vector<Draw*>& slots = Slots();
			size_t i = 0;
			do
			{
				std::string line = std::to_string( slots.size() ) + " " + std::to_string( i );
				for (size_t n = 0; (n < DRAWS_LINE) && (i < slots.size()); n++, i++)
				{
					char vbuf[32];
					snprintf( vbuf, sizeof vbuf, " %.17g", slots[i]->u );
					line += vbuf;
				}
				v->RecordEvent( DRAWS, line.c_str() );
			} while (i < slots.size());
		}

		Track();
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
		Track();
		if (!event) event = "";
		if (!_stricmp( type, NOMINAL ))
		{
			if (!locked && v->Playback())
			{
				const int format = atoi( event );
				if ((format != 1) && (format != FORMAT))
				{
					static std::set<std::string> logged;
					if (logged.insert( v->GetName() ).second)
						oapiWriteLogV( "(SSV) [WARNING] Flight recorder: %s recorded in format %d, not 1 or %d: not played back nominal", v->GetName(), format, FORMAT );
					return true;
				}
				locked = true;
				nominal++;
				if ((format == 1) && (mode != Mode::LIVE)) mode = Mode::NOMINAL;
				oapiWriteLogV( "(SSV) [INFO] Flight recorder: %s plays back nominal%s (format %d)", v->GetName(), (format == 1) ? "" : ", the recorded draws kept", format );
			}
			return true;
		}
		if (!_stricmp( type, DRAWS ))
		{
			if (!locked || (mode == Mode::NOMINAL) || (mode == Mode::LIVE)) return true;
			std::vector<Draw*>& slots = Slots();
			char* e = nullptr;
			const long count = strtol( event, &e, 10 );
			const char* p = e;
			long i = strtol( p, &e, 10 );
			if ((e == p) || (count < 0) || (i < 0)) return true;
			if (count != static_cast<long>(slots.size()))
			{
				mode = Mode::NOMINAL;
				oapiWriteLogV( "(SSV) [WARNING] Flight recorder: %s recorded %ld kept draws, %zu now: played back nominal", v->GetName(), count, slots.size() );
				return true;
			}
			if (i > count) return true;
			for (p = e; i < count; i++, p = e)
			{
				const double u = strtod( p, &e );
				if (e == p) break;
				slots[i]->u = u;
			}
			if ((i >= count) && (mode == Mode::WAIT)) mode = Mode::DRAWN;
			return true;
		}
		if (!_stricmp( type, DRAW ))
		{
			if (!locked || (mode == Mode::NOMINAL) || (mode == Mode::LIVE)) return true;
			std::vector<Draw*>& slots = Slots();
			char* e = nullptr;
			const long i = strtol( event, &e, 10 );
			const char* p = e;
			const double u = strtod( p, &e );
			if ((e != p) && (i >= 0) && (i < static_cast<long>(slots.size()))) slots[i]->u = u;
			return true;
		}
		return !_stricmp( type, EVENT ) || !_stricmp( type, CW ) || !_stricmp( type, MA );
	}

	bool Recorder::Recording( void ) const
	{
		return v->Recording();
	}

	bool Recorder::PlayingBack( void ) const
	{
		return v->Playback();
	}

	void Recorder::Record( const char* type, const std::string& text ) const
	{
		if (v->Recording()) v->RecordEvent( type, OneLine( text ).c_str() );
		return;
	}
}
