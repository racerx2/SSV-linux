/****************************************************************************
  This file is part of Space Shuttle Vessel

  Abort moding (MSC abort declaration and AOA/ATO targeting scheduling) definition


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
#ifndef _dps_ABORTMODING_H_
#define _dps_ABORTMODING_H_


#include "../SimpleGPCSoftware.h"


namespace dps
{
	class AscentDAP;
	class OMSBurnSoftware;
	class AOA_ATO_TGT;

	// The abort part of the MSC: declares ATO and AOA from the ABORT MODE switch, the OVERRIDE display and the maneuver display, and schedules AOA/ATO targeting.
	// TAL and RTLS requests are recognized but not declared until their phases.
	class AbortModing : public SimpleGPCSoftware
	{
		private:
			AscentDAP* pAscentDAP;
			OMSBurnSoftware* pOMSBurnSoftware;
			AOA_ATO_TGT* pAOA_ATO_TGT;

			bool prevReq[3];// ATO, TAL, RTLS switch requests last pass
			bool tgtItemUsed;// ITEM 35 was used: the switch and OVERRIDE ATO selections are ignored
			bool logged[2];// TAL, RTLS not available, logged once

			void Reset( void );
			void DeclareATO( void );
			void NotAvailable( int k );

		public:
			explicit AbortModing( SimpleGPCSystem* _gpc );
			virtual ~AbortModing( void );

			void Realize( void ) override;
			void OnPreStep( double simt, double simdt, double mjd ) override;
			bool OnMajorModeChange( unsigned int newMajorMode ) override;
			bool OnParseLine( const char* keyword, const char* value ) override;
			void OnSaveState( FILEHANDLE scn ) const override;

			// 0 none, 1 ATO, 2 AOA, 3 TAL, 4 RTLS
			unsigned short GetAbortMode( void ) const;
	};
}


#endif// _dps_ABORTMODING_H_
