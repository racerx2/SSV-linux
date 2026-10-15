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
#ifndef _CREWCONTROLS_H_
#define _CREWCONTROLS_H_


#include <string>


// the controller positions UpdateControllers sets: RHC, THC, trims, RPTA, SBTC, brakes and the RMS hand controllers
struct CrewControls
{
	double rhc[3][3] = {};// left, right, aft: pitch, roll, yaw
	short thc[2][3] = {};// left, aft: X, Y, Z
	short trim[2][2] = {};// left, right: pitch, roll
	double rpta = 0.0;
	double sbtc[2] = {1.0, 1.0};// left, right
	bool sbtcto[2] = {};// left, right
	double brake[2][2] = {};// CDR, PLT: left, right
	double rmsrhc[3] = {};// RMS RHC inputs, as RMS_RHCInput (-1..1)
	double rmsthc[3] = {};// RMS THC inputs, as RMS_THCInput (-1..1)
	short rmssj = 0;// RMS single joint drive: -1, 0, 1
};

// the 35 values in the order above, one line
std::string FormatCrewControls( const CrewControls& c );

// false if a value is missing or out of range
bool ParseCrewControls( const char* s, CrewControls& c );

// worth recording: a discrete changed, or an analog value moved more than 0.005 or reached 0 or +-1
bool CrewControlsDiffer( const CrewControls& last, const CrewControls& now );


#endif// _CREWCONTROLS_H_
