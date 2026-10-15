/****************************************************************************
  This file is part of Space Shuttle Vessel

  OMS/RCS aft propellant feed valves definition


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
#ifndef __OMSRCSFEED_H_
#define __OMSRCSFEED_H_


#include "../AtlantisSubsystem.h"
#include "OMSRCS_Interface.h"
#include <DiscInPort.h>
#include <DiscOutPort.h>


using namespace discsignals;


namespace oms
{
	// travel time of a motor valve [s]
	constexpr double OMSRCS_VLV_TRAVEL = 1.0;

	// aft OMS/RCS propellant valves (GPC commanded, open/closed indications) and the aft RCS source they give
	class OMSRCSFeed:public AtlantisSubsystem
	{
		private:
			DiscInPort cmdOpen[2][VLV_COUNT];
			DiscInPort cmdClose[2][VLV_COUNT];
			DiscOutPort indOpen[2][VLV_COUNT];
			DiscOutPort indClosed[2][VLV_COUNT];

			// position: 0 closed, 1 open
			double pos[2][VLV_COUNT];

			bool fromOMS[2][2];

			bool IsOpen( unsigned int side, OMSRCS_VLV v ) const;
			bool IsClosed( unsigned int side, OMSRCS_VLV v ) const;

		public:
			explicit OMSRCSFeed( AtlantisSubsystemDirector* _director );
			virtual ~OMSRCSFeed( void );

			void Realize( void ) override;
			void OnPreStep( double simt, double simdt, double mjd ) override;
			bool OnParseLine( const char* line ) override;
			void OnSaveState( FILEHANDLE scn ) const override;

			// true if a manifold group (0 1/2, 1 3/4/5) of a pod (0 left, 1 right) is fed from the OMS tanks
			bool GroupFeedFromOMS( unsigned int side, unsigned int group ) const;

			// true if a manifold group has no source: tank isolation closed and no OMS path through its crossfeed
			bool GroupIsolated( unsigned int side, unsigned int group ) const;
	};
}


#endif// __OMSRCSFEED_H_
