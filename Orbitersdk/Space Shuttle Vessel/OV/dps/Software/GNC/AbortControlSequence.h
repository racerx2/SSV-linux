/****************************************************************************
  This file is part of Space Shuttle Vessel

  Abort Control Sequence (FSSR 4.192) definition


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
#ifndef _dps_ABORTCONTROLSEQUENCE_H_
#define _dps_ABORTCONTROLSEQUENCE_H_


#include "../SimpleGPCSoftware.h"
#include <DiscOutPort.h>


namespace dps
{
	// percent of an OMS pod per second of a two OMS engine dump (STS 83-0020 4.2.31.2 P)
	constexpr double OMS_DUMP_PCT_PER_SEC = 0.14971;

	// Abort Control Sequence (STS 83-0026 4.3.1), steps 1 to 14: OMS assist, ATO and manual pre-MECO dumps, OMS/RCS interconnect requests, Mode 2 return at MECO.
	// Owns the OMS engine commands in MM 102 and 103, and the aft RCS dump jet commands.
	class AbortControlSequence : public SimpleGPCSoftware
	{
		private:
			DiscOutPort omsCmd[2];
			DiscOutPort dumpCmd[5];// AFT_RCS_DUMP: +X 1/2, +X 3/4/5, null 1/2, null 3/4/5, null pair across the groups
			bool omsCmdOut;

			// I-loads
			int OMSASS;// OMS assist enabled
			double ASSIST_OMS_DT;// OMS assist dump timer (SSV I-load NOMTM) [s]
			double T_ASSIST;// OMS assist start after SRB separation [s]
			double ATO_OMS_DT;
			int ATO_ICNCT_SEL;
			double TAL_OMS_DT;
			int TAL_ICNCT_SEL;
			double RTLS_OMS_DT;
			int RTLS_ICNCT_SEL;
			double MANUAL_OMS_DT;// manual OMS dump timer I-load [s]
			double ICNCT_DELAY;// delay from interconnect complete to RCS jets on [s]
			double RCS_24_JET_FU_BIAS;// OMS equivalent on time added per 80 ms pass with 24 jets on [s]
			double RCS_24_JET_FU_SCALE;
			double RCS_10_JET_FU_BIAS;
			double RCS_10_JET_FU_SCALE;
			bool bATOILoad;// ATO_OMS_DT from the mission, else from the OMS load at liftoff
			bool bManualILoad;// MANUAL_OMS_DT from the mission, else from the OMS load at liftoff

			// internal
			bool burnTimeSelCplt;
			bool preMecoIcnctCplt;
			bool atoSelected;
			bool dumpCutoff;
			bool dumpEnaFirst;
			bool talFirst;
			bool assistFirst;
			bool assistEvent;
			bool serc1AFirst;
			bool step12First;
			bool step14First;
			bool mode2Done;
			bool omsDelayRunning;
			double omsDelayTimer;
			bool icnctDelayRunning;
			double icnctDelayTimer;
			double tSRBSep;// MET of SRB separation [s]
			double tCycle;

			void OPS1Init( void );
			void DumpTimersFromLoad( void );
			void Cycle( double dt );
			void Step7( double dt );
			void Step9( double dt );
			void Step12( void );
			void Outputs( void );

		public:
			explicit AbortControlSequence( SimpleGPCSystem* _gpc );
			virtual ~AbortControlSequence( void );

			void Realize( void ) override;
			void ReadILOADs( const std::map<std::string,std::string>& ILOADs ) override;
			void OnPreStep( double simt, double simdt, double mjd ) override;
			bool OnMajorModeChange( unsigned int newMajorMode ) override;
			bool OnParseLine( const char* keyword, const char* value ) override;
			void OnSaveState( FILEHANDLE scn ) const override;

			// manual OMS dump I-load (item 9 entry limit) [s]
			double GetManualOMSDTILoad( void ) const { return MANUAL_OMS_DT; };
	};
}


#endif// _dps_ABORTCONTROLSEQUENCE_H_
