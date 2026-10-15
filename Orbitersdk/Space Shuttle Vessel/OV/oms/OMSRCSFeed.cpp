/****************************************************************************
  This file is part of Space Shuttle Vessel

  OMS/RCS aft propellant feed valves


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
#include "OMSRCSFeed.h"
#include "../Atlantis.h"
#include <MathSSV.h>
#include <cassert>


namespace oms
{
	OMSRCSFeed::OMSRCSFeed( AtlantisSubsystemDirector* _director ):AtlantisSubsystem( _director, "OMSRCSFeed" )
	{
		for (unsigned int s = 0; s < 2; s++)
		{
			// ascent configuration: RCS tanks feed the RCS, crossfeeds closed
			pos[s][VLV_TK12] = 1.0;
			pos[s][VLV_TK345] = 1.0;
			pos[s][VLV_XFD12] = 0.0;
			pos[s][VLV_XFD345] = 0.0;
			pos[s][VLV_OMSXFDA] = 0.0;
			pos[s][VLV_OMSXFDB] = 0.0;
			feed[s][0] = feed[s][1] = AFTFEED_RCS;
		}
		return;
	}

	OMSRCSFeed::~OMSRCSFeed( void )
	{
	}

	void OMSRCSFeed::Realize( void )
	{
		for (unsigned int s = 0; s < 2; s++)
		{
			DiscreteBundle* pCmd = BundleManager()->CreateBundle( OMSRCS_VLV_CMD_BUNDLE[s], OMSRCS_VLV_BUNDLE_LINES );
			DiscreteBundle* pInd = BundleManager()->CreateBundle( OMSRCS_VLV_IND_BUNDLE[s], OMSRCS_VLV_BUNDLE_LINES );
			for (unsigned int v = 0; v < VLV_COUNT; v++)
			{
				cmdOpen[s][v].Connect( pCmd, OMSRCS_VLV_CH_OPEN( v ) );
				cmdClose[s][v].Connect( pCmd, OMSRCS_VLV_CH_CLOSE( v ) );
				indOpen[s][v].Connect( pInd, OMSRCS_VLV_CH_OPEN( v ) );
				indClosed[s][v].Connect( pInd, OMSRCS_VLV_CH_CLOSE( v ) );
			}
		}

		for (unsigned int s = 0; s < 2; s++)
		{
			for (unsigned int g = 0; g < 2; g++)
			{
				feed[s][g] = GroupFeed( s, g );
				STS()->SetAftRCSFeed( s, g, feed[s][g] );
			}
		}
		return;
	}

	bool OMSRCSFeed::IsOpen( unsigned int side, OMSRCS_VLV v ) const
	{
		return pos[side][v] >= 1.0;
	}

	bool OMSRCSFeed::IsClosed( unsigned int side, OMSRCS_VLV v ) const
	{
		return pos[side][v] <= 0.0;
	}

	bool OMSRCSFeed::GroupFeedFromOMS( unsigned int side, unsigned int group ) const
	{
		assert( (side <= 1) && (group <= 1) && "OMSRCSFeed::GroupFeedFromOMS" );
		// the aft crossfeed line joins both pods, so an open OMS crossfeed valve of either pod feeds it
		bool oms = IsOpen( 0, VLV_OMSXFDA ) || IsOpen( 0, VLV_OMSXFDB ) || IsOpen( 1, VLV_OMSXFDA ) || IsOpen( 1, VLV_OMSXFDB );
		if (group == 0) return oms && IsClosed( side, VLV_TK12 ) && IsOpen( side, VLV_XFD12 );
		else return oms && IsClosed( side, VLV_TK345 ) && IsOpen( side, VLV_XFD345 );
	}

	int OMSRCSFeed::CrossfeedTank( unsigned int side, unsigned int group ) const
	{
		assert( (side <= 1) && (group <= 1) && "OMSRCSFeed::CrossfeedTank" );
		const OMSRCS_VLV tk[2] = {VLV_TK12, VLV_TK345};
		const OMSRCS_VLV xfd[2] = {VLV_XFD12, VLV_XFD345};
		if (!IsOpen( side, xfd[group] )) return -1;
		// the crossfeed line joins the crossfeed valves of both groups of both pods
		const unsigned int other[3][2] = {{side, 1 - group}, {1 - side, 0}, {1 - side, 1}};
		for (const auto& o : other)
		{
			if (!IsClosed( o[0], tk[o[1]] ) && IsOpen( o[0], xfd[o[1]] )) return static_cast<int>(o[0]);
		}
		return -1;
	}

	bool OMSRCSFeed::GroupIsolated( unsigned int side, unsigned int group ) const
	{
		assert( (side <= 1) && (group <= 1) && "OMSRCSFeed::GroupIsolated" );
		return IsClosed( side, (group == 0) ? VLV_TK12 : VLV_TK345 ) && !GroupFeedFromOMS( side, group ) && (CrossfeedTank( side, group ) < 0);
	}

	AFTFEED OMSRCSFeed::GroupFeed( unsigned int side, unsigned int group ) const
	{
		if (GroupFeedFromOMS( side, group )) return AFTFEED_OMS;
		else if (GroupIsolated( side, group )) return AFTFEED_NONE;
		else if (!IsClosed( side, (group == 0) ? VLV_TK12 : VLV_TK345 )) return AFTFEED_RCS;
		else return (CrossfeedTank( side, group ) == static_cast<int>(side)) ? AFTFEED_RCS : AFTFEED_XRCS;
	}

	void OMSRCSFeed::OnPreStep( double simt, double simdt, double mjd )
	{
		for (unsigned int s = 0; s < 2; s++)
		{
			for (unsigned int v = 0; v < VLV_COUNT; v++)
			{
				bool op = cmdOpen[s][v].IsSet();
				bool cl = cmdClose[s][v].IsSet();
				if (op && !cl) pos[s][v] = range( 0.0, pos[s][v] + (simdt / OMSRCS_VLV_TRAVEL), 1.0 );
				else if (cl && !op) pos[s][v] = range( 0.0, pos[s][v] - (simdt / OMSRCS_VLV_TRAVEL), 1.0 );

				if (IsOpen( s, static_cast<OMSRCS_VLV>(v) )) indOpen[s][v].SetLine();
				else indOpen[s][v].ResetLine();
				if (IsClosed( s, static_cast<OMSRCS_VLV>(v) )) indClosed[s][v].SetLine();
				else indClosed[s][v].ResetLine();
			}

			for (unsigned int g = 0; g < 2; g++)
			{
				AFTFEED f = GroupFeed( s, g );
				if (f != feed[s][g])
				{
					feed[s][g] = f;
					STS()->SetAftRCSFeed( s, g, f );
				}
			}
		}
		return;
	}

	bool OMSRCSFeed::OnParseLine( const char* line )
	{
		unsigned int s;
		if (!_strnicmp( line, "VALVES_L", 8 )) s = 0;
		else if (!_strnicmp( line, "VALVES_R", 8 )) s = 1;
		else return false;

		double p[VLV_COUNT];
		if (sscanf_s( line + 8, "%lf %lf %lf %lf %lf %lf", &p[0], &p[1], &p[2], &p[3], &p[4], &p[5] ) == VLV_COUNT)
		{
			for (unsigned int v = 0; v < VLV_COUNT; v++) pos[s][v] = range( 0.0, p[v], 1.0 );
		}
		return true;
	}

	void OMSRCSFeed::OnSaveState( FILEHANDLE scn ) const
	{
		char cbuf[128];
		const char* key[2] = {"VALVES_L", "VALVES_R"};
		for (unsigned int s = 0; s < 2; s++)
		{
			sprintf_s( cbuf, 128, "%.2f %.2f %.2f %.2f %.2f %.2f", pos[s][0], pos[s][1], pos[s][2], pos[s][3], pos[s][4], pos[s][5] );
			oapiWriteScenario_string( scn, key[s], cbuf );
		}
		return;
	}
}
