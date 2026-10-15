/****************************************************************************
  This file is part of Space Shuttle Vessel

  Orbiter vehicle flight recorder support


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
#include "Atlantis.h"
#include "Atlantis_vc_defs.h"
#include "CrewControls.h"
#include <FlightRecorder.h>
#include <MathSSV.h>
#include <cstdio>


namespace
{
	void LogUnknown( const char* type, const std::string& key )
	{
		flightrec::LogUnknown( "SSV_OV", type, key );
	}
}

bool Atlantis::FlightRecorderPlayback( double simt, double event_t, const char* event_type, const char* event )
{
	if (!event) event = "";
	if (flightrecorder.OnPlaybackEvent( event_type, event )) return true;

	if (!_stricmp( event_type, flightrec::VC ))
	{
		const char* p = event;
		std::string panel, component;
		if (!flightrec::NextToken( p, panel ) || !flightrec::NextToken( p, component ))
		{
			LogUnknown( event_type, event );
			return true;
		}
		const char* state = flightrec::SkipBlanks( p );
		vc::PanelGroup<Atlantis>* groups[] = {pgForward, pgLeft, pgCenter, pgRight, pgOverhead, pgOverheadAft, pgAftPort, pgAft, pgAftStbd};
		for (vc::PanelGroup<Atlantis>* pg : groups)
		{
			if (pg->OnPlaybackEvent( panel, component, simt, event_t, event_type, state )) return true;
		}
		LogUnknown( event_type, flightrec::Quote( panel ) + " " + flightrec::Quote( component ) );
		return true;
	}

	if (!_stricmp( event_type, flightrec::CTRL ))
	{
		CrewControls c;
		if (ParseCrewControls( event, c )) SetCrewControls( c );
		else LogUnknown( event_type, "(bad line)" );
		return true;
	}

	if (!_stricmp( event_type, flightrec::CMD ))
	{
		if (!ApplyCrewCommand( event ))
		{
			const char* p = event;
			std::string word;
			flightrec::NextToken( p, word );
			LogUnknown( event_type, word );
		}
		return true;
	}
	return false;
}

void Atlantis::CrewCommand( const std::string& cmd )
{
	if (flightrecorder.Locked()) return;
	flightrecorder.Record( flightrec::CMD, cmd );
	ApplyCrewCommand( cmd.c_str() );
	return;
}

bool Atlantis::ApplyCrewCommand( const char* cmd )
{
	const char* p = cmd;
	std::string word, arg;
	if (!flightrec::NextToken( p, word )) return false;

	if (word == "GEAR")
	{
		if (!flightrec::NextToken( p, arg )) return false;
		if (arg == "ARM") ManLandingGearArm();
		else if (arg == "DOWN") ManLandingGearDown();
		else return false;
		return true;
	}
	if (word == "SSMEPB")
	{
		for (int i = 0; i < 3; i++) SSMEPBAnalog[i].SetLine();
		return true;
	}
	if (word == "RMS")
	{
		int on = 0;
		if (!pRMS || (sscanf_s( p, "%d", &on ) != 1)) return false;
		ControlRMS = (on != 0);
		if (ControlRMS) DisplayCameraLabel( TEXT_RMSCONTROL );
		else DisplayCameraLabel( TEXT_RCSCONTROL );
		return true;
	}
	if (word == "RMSEE")
	{
		if (!flightrec::NextToken( p, arg )) return false;
		if (arg == "GRAPPLE")
		{
			RMSGrapple.SetLine();
			RMSRelease.ResetLine();
		}
		else if (arg == "RELEASE")
		{
			RMSGrapple.ResetLine();
			RMSRelease.SetLine();
		}
		else if (arg == "OFF")
		{
			RMSGrapple.ResetLine();
			RMSRelease.ResetLine();
		}
		else return false;
		return true;
	}
	if (word == "RMSRATE")
	{
		int on = 0;
		if (sscanf_s( p, "%d", &on ) != 1) return false;
		if (on) RMSCoarseRateOut.SetLine();
		else RMSCoarseRateOut.ResetLine();
		return true;
	}
	if (word == "PLBCAM")
	{
		int f[4] = {0, 0, 0, 0};
		if (sscanf_s( p, "%d %d %d %d", &f[0], &f[1], &f[2], &f[3] ) != 4) return false;
		bPLBCamPanLeft_Man = (f[0] != 0);
		bPLBCamPanRight_Man = (f[1] != 0);
		bPLBCamTiltUp_Man = (f[2] != 0);
		bPLBCamTiltDown_Man = (f[3] != 0);
		return true;
	}
	return false;
}

std::string Atlantis::PLBCamCommand( int idx, bool on ) const
{
	bool f[4] = {bPLBCamPanLeft_Man, bPLBCamPanRight_Man, bPLBCamTiltUp_Man, bPLBCamTiltDown_Man};
	f[idx] = on;
	char cbuf[32];
	sprintf_s( cbuf, sizeof(cbuf), "PLBCAM %d %d %d %d", f[0] ? 1 : 0, f[1] ? 1 : 0, f[2] ? 1 : 0, f[3] ? 1 : 0 );
	return cbuf;
}

CrewControls Atlantis::GetCrewControls( void ) const
{
	CrewControls c;
	for (int i = 0; i < 3; i++)
	{
		c.rhc[0][i] = range( -1.0, LeftRHC[i], 1.0 );
		c.rhc[1][i] = range( -1.0, RightRHC[i], 1.0 );
		c.rhc[2][i] = range( -1.0, AftRHC[i], 1.0 );
		c.thc[0][i] = LeftTHC[i];
		c.thc[1][i] = AftTHC[i];
		c.rmsrhc[i] = range( -1.0, RMS_RHCInput[i].GetVoltage() / 5.0, 1.0 );
		c.rmsthc[i] = range( -1.0, RMS_THCInput[i].GetVoltage() / 5.0, 1.0 );
	}
	for (int i = 0; i < 2; i++)
	{
		c.trim[0][i] = LeftRHCTrim[i];
		c.trim[1][i] = RightRHCTrim[i];
		c.brake[0][i] = range( 0.0, BrakeCDR[i], 1.0 );
		c.brake[1][i] = range( 0.0, BrakePLT[i], 1.0 );
	}
	c.rpta = range( -1.0, RPTAs, 1.0 );
	c.sbtc[0] = range( 0.0, LeftSBTC, 1.0 );
	c.sbtc[1] = range( 0.0, RightSBTC, 1.0 );
	c.sbtcto[0] = LeftSBTCTO;
	c.sbtcto[1] = RightSBTCTO;
	c.rmssj = static_cast<short>(lastRMSSJCommand);
	return c;
}

void Atlantis::SetCrewControls( const CrewControls& c )
{
	for (int i = 0; i < 3; i++)
	{
		LeftRHC[i] = c.rhc[0][i];
		RightRHC[i] = c.rhc[1][i];
		AftRHC[i] = c.rhc[2][i];
		LeftTHC[i] = c.thc[0][i];
		AftTHC[i] = c.thc[1][i];
	}
	for (int i = 0; i < 2; i++)
	{
		LeftRHCTrim[i] = c.trim[0][i];
		RightRHCTrim[i] = c.trim[1][i];
		BrakeCDR[i] = c.brake[0][i];
		BrakePLT[i] = c.brake[1][i];
	}
	RPTAs = c.rpta;
	LeftSBTC = c.sbtc[0];
	RightSBTC = c.sbtc[1];
	LeftSBTCTO = c.sbtcto[0];
	RightSBTCTO = c.sbtcto[1];

	// the RMS lines as UpdateControllers sets them; the single joint lines only on a change, so the A8 switch works
	if (ControlRMS)
	{
		for (int i = 0; i < 3; i++)
		{
			RMS_RHCInput[i].SetLine( 5.0f * static_cast<float>(c.rmsrhc[i]) );
			RMS_THCInput[i].SetLine( 5.0f * static_cast<float>(c.rmsthc[i]) );
		}
		if (c.rmssj != lastRMSSJCommand)
		{
			if (c.rmssj > 0)
			{
				RMSDrivePlus.SetLine();
				RMSDriveMinus.ResetLine();
			}
			else if (c.rmssj < 0)
			{
				RMSDrivePlus.ResetLine();
				RMSDriveMinus.SetLine();
			}
			else
			{
				RMSDrivePlus.ResetLine();
				RMSDriveMinus.ResetLine();
			}
			lastRMSSJCommand = c.rmssj;
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			RMS_RHCInput[i].ResetLine();
			RMS_THCInput[i].ResetLine();
		}
	}
	return;
}

void Atlantis::RecordCrewControls( void )
{
	if (!flightrecorder.Recording()) return;
	const CrewControls c = GetCrewControls();
	const bool start = flightrecorder.JustStarted();
	if (!start && !CrewControlsDiffer( crewctrl_rec, c )) return;
	if (start)
	{
		// state only crew commands change, so a playback from here starts like the recording
		if (pRMS) flightrecorder.Record( flightrec::CMD, ControlRMS ? "RMS 1" : "RMS 0" );
		flightrecorder.Record( flightrec::CMD, RMSCoarseRateIn ? "RMSRATE 1" : "RMSRATE 0" );
		flightrecorder.Record( flightrec::CMD, PLBCamCommand( 0, bPLBCamPanLeft_Man ) );
	}
	crewctrl_rec = c;
	flightrecorder.Record( flightrec::CTRL, FormatCrewControls( c ) );
	return;
}
