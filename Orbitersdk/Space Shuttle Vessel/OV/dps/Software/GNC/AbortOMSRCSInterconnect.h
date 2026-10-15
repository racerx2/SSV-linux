/****************************************************************************
  This file is part of Space Shuttle Vessel

  Abort OMS/RCS Interconnect (FSSR 4.184) definition


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
#ifndef _dps_ABORTOMSRCSINTERCONNECT_H_
#define _dps_ABORTOMSRCSINTERCONNECT_H_


#include "../SimpleGPCSoftware.h"
#include <DiscInPort.h>
#include <DiscOutPort.h>


namespace dps
{
	// Abort OMS/RCS interconnect sequence (STS 83-0026 4.3.2): feeds OMS propellant to the aft RCS manifolds and returns them to normal.
	// Valve commands and indications through the OMSRCS_VLV bundles; commfault and the OMS B mask are not modeled.
	class AbortOMSRCSInterconnect : public SimpleGPCSoftware
	{
		private:
			// valve groups of a pod, as oms::OMSRCS_VLV
			enum VLV { TK12, TK345, XFD12, XFD345, OXA, OXB, VLV_COUNT };

			// sequence steps
			enum STEP { IDLE, IC_TK_CLOSE, IC_XFD_OPEN, IC_XFD_CHECK, IC_RECONFIG_XFD, IC_RECONFIG_TK, IC_OMS_XFD_A, IC_COMPLETE,
				RTN_OMS_XFD_CLOSE, RTN_TK_OPEN, RTN_XFD, M2_TK_OPEN, M2_XFD, M2_OMS_XFD_CLOSE };

			DiscOutPort cmdOpen[2][VLV_COUNT];
			DiscOutPort cmdClose[2][VLV_COUNT];
			DiscInPort indOpen[2][VLV_COUNT];
			DiscInPort indClosed[2][VLV_COUNT];

			STEP step;
			double stepTimer;// time since the current step started [s]
			bool stepFirstPass;
			bool icInProgress;
			bool rtnInProgress;
			bool monitor;
			bool xfdIsoFail;
			bool bOXA;// OMS crossfeed A valves commanded because a B valve did not open
			int icFail;
			int tkPosCount[2][2];// monitor: consecutive passes without a closed indication, [pod][group]
			unsigned int cmdState[2];// held valve commands, bit v open, bit v + VLV_COUNT close
			double tCycle;

			void TerminateAll( void );
			void Issue( unsigned int side, VLV v, bool open );
			void Terminate( unsigned int side, VLV v, bool open );
			bool Open( unsigned int side, VLV v ) const;
			bool Closed( unsigned int side, VLV v ) const;
			void Goto( STEP s );
			void SetAllJetInh( bool inh );
			bool Monitor( void );
			void Cycle( double dt );
			void Step16( void );

		public:
			explicit AbortOMSRCSInterconnect( SimpleGPCSystem* _gpc );
			virtual ~AbortOMSRCSInterconnect( void );

			void Realize( void ) override;
			void OnPreStep( double simt, double simdt, double mjd ) override;
			bool OnMajorModeChange( unsigned int newMajorMode ) override;
			bool OnParseLine( const char* keyword, const char* value ) override;
			void OnSaveState( FILEHANDLE scn ) const override;
	};
}


#endif// _dps_ABORTOMSRCSINTERCONNECT_H_
