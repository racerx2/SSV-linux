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
#ifndef _FLIGHTRECORDER_H_
#define _FLIGHTRECORDER_H_


#include <OrbiterAPI.h>
#include <VesselAPI.h>
#include <string>


namespace flightrec
{
	// event types, one token each
	inline constexpr char NOMINAL[] = "SSV_NOMINAL";
	inline constexpr char VC[] = "SSV_VC";
	inline constexpr char CTRL[] = "SSV_CTRL";
	inline constexpr char CMD[] = "SSV_CMD";
	inline constexpr char EVENT[] = "SSV_EVENT";
	inline constexpr char CW[] = "SSV_CW";
	inline constexpr char MA[] = "SSV_MA";

	// format of the SSV events, written with SSV_NOMINAL
	inline constexpr int FORMAT = 1;

	// a draw in [0,1), or 0.5 while a vessel of this module plays back nominal
	double Rand( void );

	// a draw kept from earlier: u, or 0.5 while nominal
	double Held( double u );

	bool Nominal( void );

	// a name in double quotes, as in the scenario's panel blocks
	std::string Quote( const std::string& s );

	// next word or "quoted words" from p; p moves past it; false at the end or on an open quote
	bool NextToken( const char*& p, std::string& out );

	// p past spaces and tabs
	const char* SkipBlanks( const char* p );

	// text for one .atc line: control characters become spaces
	std::string OneLine( const std::string& s );

	// an event nothing plays back, logged once per type and key
	void LogUnknown( const char* module, const char* type, const std::string& key );


	class Recorder
	{
		private:
			VESSEL* v;
			int steps;// steps of this recording, up to 2
			bool started;
			bool locked;

		public:
			explicit Recorder( VESSEL* v );
			~Recorder( void );

			// once a frame, first thing in clbkPreStep (or clbkPostStep): the marker on the second recorded step, as Orbiter empties the .atc after the first; the end of a playback
			void Step( void );

			// SSV_NOMINAL and the notes; true when handled
			bool OnPlaybackEvent( const char* type, const char* event );

			bool Recording( void ) const;

			// true in the step the marker is written: the step to record the initial state
			bool JustStarted( void ) const { return started; }

			// the recorded crew drives this vessel: live crew input is ignored
			bool Locked( void ) const { return locked; }

			void Record( const char* type, const std::string& text ) const;
	};
}


#endif// _FLIGHTRECORDER_H_
