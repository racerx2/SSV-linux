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
			fromOMS[s][0] = fromOMS[s][1] = false;
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
				fromOMS[s][g] = GroupFeedFromOMS( s, g );
				STS()->SetAftRCSFeedFromOMS( s, g, fromOMS[s][g] );
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

	bool OMSRCSFeed::GroupIsolated( unsigned int side, unsigned int group ) const
	{
		assert( (side <= 1) && (group <= 1) && "OMSRCSFeed::GroupIsolated" );
		return IsClosed( side, (group == 0) ? VLV_TK12 : VLV_TK345 ) && !GroupFeedFromOMS( side, group );
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
				bool feed = GroupFeedFromOMS( s, g );
				if (feed != fromOMS[s][g])
				{
					fromOMS[s][g] = feed;
					STS()->SetAftRCSFeedFromOMS( s, g, feed );
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
