/****************************************************************************
  This file is part of Space Shuttle Vessel

  Abort Control Sequence (STS 83-0026 4.3.1) implementation


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
#include "AbortControlSequence.h"
#include "../../../Atlantis.h"
#include "../../../ParameterValues.h"
#include "OMSBurnSoftware.h"
#include <algorithm>
#include <cassert>


namespace dps
{
	constexpr double ACS_CYCLE = 0.08;// sequence pass [s]
	constexpr double OMS_CUTOFF_DELAY = 30.0;// minimum OMS off time before a restart [s]
	// quantity per side left by the default ATO dump (STS-88 handbook full ATO dump) and manual dump [%]
	constexpr double ATO_DUMP_LEFT_PCT = 34.0;
	constexpr double MANUAL_DUMP_LEFT_PCT = 20.0;

	// OMS equivalent on time added per 80 ms pass with N aft RCS jets on, from the OMS and dump jet flow rates of this vehicle
	static double RCSJetFuBias( int jets )
	{
		double mdotOMS2 = 2.0 * OMS_THRUST / OMS_ISP0;
		double mdotJets = jets * RCS_DUMP_JET_THRUST / RCS_DUMP_JET_ISP0;
		return ACS_CYCLE * mdotJets / mdotOMS2;
	}


	AbortControlSequence::AbortControlSequence( SimpleGPCSystem* _gpc ):SimpleGPCSoftware( _gpc, "AbortControlSequence" ),
		pOMSBurnSoftware( NULL )
	{
		// I-loads: the OMS assist keeps the SSV I-load names and timing; the ATO dump is the handbook full ATO dump (JSC-10559 4.3.4.4)
		OMSASS = 1;
		ASSIST_OMS_DT = 59.0;
		T_ASSIST = 10.0;
		ATO_OMS_DT = 270.0;
		ATO_ICNCT_SEL = 1;
		TAL_OMS_DT = 208.0;
		TAL_ICNCT_SEL = 1;
		RTLS_OMS_DT = 208.0;
		RTLS_ICNCT_SEL = 1;
		MANUAL_OMS_DT = 460.0;
		ICNCT_DELAY = 0.0;
		RCS_24_JET_FU_BIAS = RCSJetFuBias( 24 );
		RCS_24_JET_FU_SCALE = 1.0 + (RCS_24_JET_FU_BIAS / ACS_CYCLE);
		RCS_10_JET_FU_BIAS = RCSJetFuBias( 10 );
		RCS_10_JET_FU_SCALE = 1.0 + (RCS_10_JET_FU_BIAS / ACS_CYCLE);
		bATOILoad = false;
		bManualILoad = false;

		Reset();
		return;
	}

	void AbortControlSequence::Reset( void )
	{
		burnTimeSelCplt = false;
		preMecoIcnctCplt = false;
		atoSelected = false;
		dumpCutoff = false;
		dumpEnaFirst = true;
		talFirst = true;
		assistFirst = true;
		assistEvent = false;
		serc1AFirst = true;
		step12First = true;
		step14First = true;
		mode2Done = false;
		omsDelayRunning = false;
		omsDelayTimer = 0.0;
		icnctDelayRunning = false;
		icnctDelayTimer = 0.0;
		tSRBSep = -1.0;
		tCycle = 0.0;
		return;
	}

	AbortControlSequence::~AbortControlSequence( void )
	{
		return;
	}

	void AbortControlSequence::Realize( void )
	{
		// the OMS engine command lines have one driver, OMSBurnSoftware, which also restores the abort OMS ignition after a load
		pOMSBurnSoftware = dynamic_cast<OMSBurnSoftware*>(FindSoftware( "OMSBurnSoftware" ));
		assert( (pOMSBurnSoftware != NULL) && "AbortControlSequence::Realize.pOMSBurnSoftware" );

		DiscreteBundle* pBundle = BundleManager()->CreateBundle( "AFT_RCS_DUMP", 16 );
		for (int i = 0; i < 5; i++) dumpCmd[i].Connect( pBundle, i );
		return;
	}

	void AbortControlSequence::ReadILOADs( const std::map<std::string,std::string>& ILOADs )
	{
		GetValILOAD( "OMSASS", ILOADs, OMSASS );
		GetValILOAD( "NOMTM", ILOADs, ASSIST_OMS_DT );
		GetOptILOAD( "T_ASSIST", ILOADs, T_ASSIST );
		bATOILoad = GetOptILOAD( "ATO_OMS_DT", ILOADs, ATO_OMS_DT );
		GetOptILOAD( "ATO_ICNCT_SEL", ILOADs, ATO_ICNCT_SEL );
		GetOptILOAD( "TAL_OMS_DT", ILOADs, TAL_OMS_DT );
		GetOptILOAD( "TAL_ICNCT_SEL", ILOADs, TAL_ICNCT_SEL );
		GetOptILOAD( "RTLS_OMS_DT", ILOADs, RTLS_OMS_DT );
		GetOptILOAD( "RTLS_ICNCT_SEL", ILOADs, RTLS_ICNCT_SEL );
		bManualILoad = GetOptILOAD( "MANUAL_OMS_DT", ILOADs, MANUAL_OMS_DT );
		GetOptILOAD( "ICNCT_DELAY", ILOADs, ICNCT_DELAY );
		GetOptILOAD( "RCS_24_JET_FU_BIAS", ILOADs, RCS_24_JET_FU_BIAS );
		GetOptILOAD( "RCS_24_JET_FU_SCALE", ILOADs, RCS_24_JET_FU_SCALE );
		GetOptILOAD( "RCS_10_JET_FU_BIAS", ILOADs, RCS_10_JET_FU_BIAS );
		GetOptILOAD( "RCS_10_JET_FU_SCALE", ILOADs, RCS_10_JET_FU_SCALE );

		OPS1Init();
		return;
	}

	void AbortControlSequence::OPS1Init( void )
	{
		// SPEC 51 PRPLT DUMP items: ICNCT ENA, ARM/START/STOP reset, dump time from the manual I-load (STS 83-0020 4.2.31.1)
		WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 1 );
		WriteCOMPOOL_IS( SCP_OVRD_DUMP_ARM, 0 );
		WriteCOMPOOL_IS( SCP_ORBITER_DUMP_ENA, 0 );
		WriteCOMPOOL_IS( SCP_ORBITER_DUMP_INH, 0 );
		WriteCOMPOOL_SS( SCP_MANUAL_OMS_DT, static_cast<float>(MANUAL_OMS_DT) );
		WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>(MANUAL_OMS_DT) );
		WriteCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME, 0.0f );
		WriteCOMPOOL_SS( SCP_OMS_TIME_SCALE_FACTOR, 1.0f );
		return;
	}

	void AbortControlSequence::DumpTimersFromLoad( void )
	{
		// timers not in the mission's I-loads follow its OMS load, so a dump leaves the same quantity as on a flight with those I-loads
		double pct = 100.0 * STS()->GetOMSPropellantMass() / (2.0 * ORBITER_MAX_OMS_PROPELLANT_MASS);
		double assist = (OMSASS == 1) ? ASSIST_OMS_DT : 0.0;
		if (!bATOILoad) ATO_OMS_DT = std::max<double>( 0.0, ((pct - ATO_DUMP_LEFT_PCT) / OMS_DUMP_PCT_PER_SEC) - assist );
		if (!bManualILoad)
		{
			MANUAL_OMS_DT = std::max<double>( 0.0, (pct - MANUAL_DUMP_LEFT_PCT) / OMS_DUMP_PCT_PER_SEC );
			WriteCOMPOOL_SS( SCP_MANUAL_OMS_DT, static_cast<float>(MANUAL_OMS_DT) );
			WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>(MANUAL_OMS_DT) );
		}
		oapiWriteLogV( "(SSV_OV) [INFO] OMS load %.0f %%/side: ATO dump %.0f s%s, manual dump %.0f s%s", pct, ATO_OMS_DT, bATOILoad ? " (I-load)" : "", MANUAL_OMS_DT, bManualILoad ? " (I-load)" : "" );
		return;
	}

	bool AbortControlSequence::OnMajorModeChange( unsigned int newMajorMode )
	{
		if ((newMajorMode == 101) && (GetMajorMode() != 0))
		{
			Reset();
			OPS1Init();
		}
		else if ((newMajorMode == 102) && (GetMajorMode() == 101)) DumpTimersFromLoad();
		else if ((newMajorMode == 103) && (GetMajorMode() == 102)) tSRBSep = STS()->GetMET();

		switch (newMajorMode / 100)
		{
			case 1:
			case 3:
			case 6:
				return true;
			default:
				return false;
		}
	}

	void AbortControlSequence::OnPreStep( double simt, double simdt, double mjd )
	{
		unsigned int mm = GetMajorMode();

		// MSC: ascent OMS assist event; SRB separation time unknown (MM 103 loaded without it): the assist is taken as done, upstream fired it 10 s after the load
		if ((mm == 103) && (OMSASS == 1) && !assistEvent && (tSRBSep >= 0.0) && (STS()->GetMET() >= (tSRBSep + T_ASSIST))) assistEvent = true;

		tCycle += simdt;
		if (tCycle < ACS_CYCLE) return;
		double dt = tCycle;
		tCycle = 0.0;

		if ((mm >= 102) && (mm <= 104)) Cycle( dt );
		Outputs();
		return;
	}

	void AbortControlSequence::Cycle( double dt )
	{
		if (omsDelayRunning) omsDelayTimer += dt;
		if (icnctDelayRunning) icnctDelayTimer += dt;

		bool serc = (ReadCOMPOOL_IS( SCP_SERC_FLAG ) != 0);

		// step 1
		WriteCOMPOOL_SS( SCP_OMS_TIME_SCALE_FACTOR, 1.0f );
		if (serc)
		{
			// step 1A: interconnect for single engine roll control (OMS helium and vapor isolation valves are not modeled; the 2 EO mode 5 delay is phase 4)
			if (ReadCOMPOOL_IS( SCP_MECO_CONFIRMED ) != 0) preMecoIcnctCplt = true;
			else if (ReadCOMPOOL_IS( SCP_GUID_MECO_PREP_FLAG ) != 0) preMecoIcnctCplt = true;
			else if (serc1AFirst)
			{
				serc1AFirst = false;
				WriteCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD, 0 );
				WriteCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD, 0 );
				WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 0 );
				WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 1 );
				WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 1 );
			}
		}

		// step 1B
		if (ReadCOMPOOL_IS( SCP_ORBITER_DUMP_ENA ) != 0)
		{
			if (dumpEnaFirst)
			{
				dumpEnaFirst = false;
				burnTimeSelCplt = true;
				WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, ReadCOMPOOL_SS( SCP_MANUAL_OMS_DT ) );
			}
		}
		else
		{
			dumpEnaFirst = true;
			if (atoSelected && (ReadCOMPOOL_IS( SCP_TAL_ABORT_DECLARED ) != 0) && talFirst)
			{
				talFirst = false;
				WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>(TAL_OMS_DT) );
			}
		}

		// step 2
		if (!(burnTimeSelCplt || preMecoIcnctCplt))
		{
			// step 3
			double assist = (OMSASS == 1) ? ASSIST_OMS_DT : 0.0;
			if (ReadCOMPOOL_IS( SCP_RTLS_ABORT_DECLARED ) != 0)
			{
				// step 4
				WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>(RTLS_OMS_DT) );
				burnTimeSelCplt = true;
				WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, ((RTLS_ICNCT_SEL != 0) || serc) ? 1 : 0 );
				return;
			}
			else if (ReadCOMPOOL_IS( SCP_TAL_ABORT_DECLARED ) != 0)
			{
				// step 5
				WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>(TAL_OMS_DT) );
				burnTimeSelCplt = true;
				WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, ((TAL_ICNCT_SEL != 0) || serc) ? 1 : 0 );
				return;
			}
			else if (ReadCOMPOOL_IS( SCP_S_ABORT_CONTROL ) != 0)
			{
				// step 6
				WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>((ReadCOMPOOL_SS( SCP_K2 ) * ATO_OMS_DT) + assist) );
				WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, ((ATO_ICNCT_SEL != 0) || serc) ? 1 : 0 );
				burnTimeSelCplt = true;
				atoSelected = true;
				return;
			}
			else
			{
				// step 6a: OMS assist, through the OMS engines only
				if (!assistEvent) return;
				if (assistFirst)
				{
					assistFirst = false;
					WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, static_cast<float>(assist) );
					if (!serc) WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 0 );
				}
			}
		}

		Step7( dt );
		return;
	}

	void AbortControlSequence::Step7( double dt )
	{
		// all abort functions are suspended for the ET separation
		if ((ReadCOMPOOL_IS( SCP_GUID_MECO_PREP_FLAG ) != 0) || (ReadCOMPOOL_IS( SCP_MECO_CONFIRMED ) != 0))
		{
			Step12();
			return;
		}

		// step 8
		bool serc = (ReadCOMPOOL_IS( SCP_SERC_FLAG ) != 0);
		if ((ReadCOMPOOL_IS( SCP_ORBITER_DUMP_INH ) == 0) && (ReadCOMPOOL_SS( SCP_OMS_DT_COMPUTED ) > ReadCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME )))
		{
			Step9( dt );
			return;
		}

		if (ReadCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD ) != 0)
		{
			WriteCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD, 0 );
			WriteCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD, 0 );
			WriteCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD, 0 );
			WriteCOMPOOL_IS( SCP_ORBITER_DUMP_ENA, 0 );
			dumpCutoff = true;
			omsDelayRunning = true;
			omsDelayTimer = 0.0;
		}
		if (serc) return;
		if (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ) != 0)
		{
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 0 );
			WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 1 );
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 0 );
		}
		return;
	}

	void AbortControlSequence::Step9( double dt )
	{
		bool serc = (ReadCOMPOOL_IS( SCP_SERC_FLAG ) != 0);

		if (ReadCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD ) != 0)
		{
			WriteCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME, static_cast<float>(ReadCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME ) + dt) );
		}
		else if (!dumpCutoff || (omsDelayTimer >= OMS_CUTOFF_DELAY))
		{
			WriteCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD, 1 );
			dumpCutoff = false;
			omsDelayRunning = false;
			omsDelayTimer = 0.0;
		}

		if (!((!serc && !dumpCutoff) || (ReadCOMPOOL_IS( SCP_ORBITER_DUMP_ENA ) != 0))) return;

		if (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA ) == 0)
		{
			if (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ) != 0)
			{
				WriteCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD, 0 );
				WriteCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD, 0 );
				WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 0 );
				WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 1 );
			}
			return;
		}

		// step 10
		if (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ) == 0)
		{
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 1 );
			WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 0 );
			icnctDelayRunning = false;
			icnctDelayTimer = 0.0;
			return;
		}
		if (!icnctDelayRunning)
		{
			icnctDelayRunning = true;
			icnctDelayTimer = 0.0;
			return;
		}
		if (icnctDelayTimer < ICNCT_DELAY) return;
		WriteCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD, 1 );
		WriteCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD, 1 );

		// step 11: OMS propellant through the RCS jets, 24 or 10 jets, groups as in Outputs()
		double k = dt / ACS_CYCLE;
		bool g12 = Group12Available();
		bool g345 = Group345Available();
		if (g12 && g345)
		{
			WriteCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME, static_cast<float>(ReadCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME ) + (RCS_24_JET_FU_BIAS * k)) );
			WriteCOMPOOL_SS( SCP_OMS_TIME_SCALE_FACTOR, static_cast<float>(RCS_24_JET_FU_SCALE) );
		}
		else if (g12 || g345)
		{
			WriteCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME, static_cast<float>(ReadCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME ) + (RCS_10_JET_FU_BIAS * k)) );
			WriteCOMPOOL_SS( SCP_OMS_TIME_SCALE_FACTOR, static_cast<float>(RCS_10_JET_FU_SCALE) );
		}
		return;
	}

	void AbortControlSequence::Step12( void )
	{
		// the MM 102 branch (3 EO contingency dump and fast separation) and the post-MECO settling burn are phase 4
		if (GetMajorMode() == 102) return;

		if (step12First)
		{
			step12First = false;
			WriteCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD, 0 );
			WriteCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD, 0 );
			WriteCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD, 0 );
		}

		// step 14: Mode 2 return to normal at MECO command
		if (ReadCOMPOOL_IS( SCP_MECO_CMD ) == 0) return;
		if (step14First)
		{
			step14First = false;
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 0 );
			WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 1 );
			WriteCOMPOOL_IS( SCP_MODE2_IND, 1 );
			WriteCOMPOOL_IS( SCP_ORBITER_DUMP_ENA, 0 );
			WriteCOMPOOL_IS( SCP_ORBITER_DUMP_INH, 0 );
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 0 );
		}
		else if (!mode2Done && (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ) == 0))
		{
			mode2Done = true;
			WriteCOMPOOL_IS( SCP_MODE2_IND, 0 );
		}

		// steps 15 to 23 are the post-MECO dumps of MM 102 (3 EO), 601 and 602, phases 3 and 4
		return;
	}

	bool AbortControlSequence::Group12Available( void ) const
	{
		return (ReadCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH ) == 0) && (ReadCOMPOOL_IS( SCP_RCS_12_ALL_JET_INH ) == 0);
	}

	bool AbortControlSequence::Group345Available( void ) const
	{
		return (ReadCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH ) == 0) && (ReadCOMPOOL_IS( SCP_RCS_345_ALL_JET_INH ) == 0);
	}

	void AbortControlSequence::Outputs( void )
	{
		// ABORT_OMS_IGN_CMD to the OMS engines, MM 102/103 included (OMSBurnSoftware is not active there)
		pOMSBurnSoftware->AbortOMSIgnCmd();

		// aft RCS dump jets, without the manifold groups that are inhibited (ASC RCS CMD SOP)
		bool px = (ReadCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD ) != 0);
		bool nl = (ReadCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD ) != 0);
		bool g12 = Group12Available();
		bool g345 = Group345Available();
		bool out[5] = {px && g12, px && g345, nl && g12, nl && g345, nl && g12 && g345};
		for (int i = 0; i < 5; i++)
		{
			if (out[i]) dumpCmd[i].SetLine();
			else dumpCmd[i].ResetLine();
		}
		return;
	}

	bool AbortControlSequence::OnParseLine( const char* keyword, const char* value )
	{
		if (!_strnicmp( keyword, "SEQ", 3 ))
		{
			int f[14] = {0};
			sscanf_s( value, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6], &f[7], &f[8], &f[9], &f[10], &f[11], &f[12], &f[13] );
			burnTimeSelCplt = (f[0] != 0);
			preMecoIcnctCplt = (f[1] != 0);
			atoSelected = (f[2] != 0);
			dumpCutoff = (f[3] != 0);
			dumpEnaFirst = (f[4] != 0);
			talFirst = (f[5] != 0);
			assistFirst = (f[6] != 0);
			assistEvent = (f[7] != 0);
			serc1AFirst = (f[8] != 0);
			step12First = (f[9] != 0);
			step14First = (f[10] != 0);
			mode2Done = (f[11] != 0);
			omsDelayRunning = (f[12] != 0);
			icnctDelayRunning = (f[13] != 0);
			return true;
		}
		else if (!_strnicmp( keyword, "TIMERS", 6 ))
		{
			// the dump timers are in the line only from MM 102, after the capture from the OMS load
			double ato = 0.0, man = 0.0;
			if (sscanf_s( value, "%lf %lf %lf %lf %lf", &omsDelayTimer, &icnctDelayTimer, &tSRBSep, &ato, &man ) == 5)
			{
				ATO_OMS_DT = ato;
				MANUAL_OMS_DT = man;
			}
			return true;
		}
		else if (!_strnicmp( keyword, "DUMP", 4 ))
		{
			float dt = 0.0f, eq = 0.0f, sf = 1.0f, man = 0.0f;
			sscanf_s( value, "%f %f %f %f", &dt, &eq, &sf, &man );
			WriteCOMPOOL_SS( SCP_OMS_DT_COMPUTED, dt );
			WriteCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME, eq );
			WriteCOMPOOL_SS( SCP_OMS_TIME_SCALE_FACTOR, sf );
			WriteCOMPOOL_SS( SCP_MANUAL_OMS_DT, man );
			return true;
		}
		else if (!_strnicmp( keyword, "CMDS", 4 ))
		{
			unsigned short c[10] = {0};
			sscanf_s( value, "%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu", &c[0], &c[1], &c[2], &c[3], &c[4], &c[5], &c[6], &c[7], &c[8], &c[9] );
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, c[0] );
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, c[1] );
			WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, c[2] );
			WriteCOMPOOL_IS( SCP_MODE2_IND, c[3] );
			WriteCOMPOOL_IS( SCP_OVRD_DUMP_ARM, c[4] );
			WriteCOMPOOL_IS( SCP_ORBITER_DUMP_ENA, c[5] );
			WriteCOMPOOL_IS( SCP_ORBITER_DUMP_INH, c[6] );
			WriteCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD, c[7] );
			WriteCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD, c[8] );
			WriteCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD, c[9] );
			return true;
		}
		return false;
	}

	void AbortControlSequence::OnSaveState( FILEHANDLE scn ) const
	{
		char cbuf[256];
		sprintf_s( cbuf, 256, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d", burnTimeSelCplt, preMecoIcnctCplt, atoSelected, dumpCutoff, dumpEnaFirst, talFirst, assistFirst, assistEvent,
			serc1AFirst, step12First, step14First, mode2Done, omsDelayRunning, icnctDelayRunning );
		oapiWriteScenario_string( scn, "SEQ", cbuf );
		if (GetMajorMode() >= 102) sprintf_s( cbuf, 256, "%.2f %.2f %.3f %.1f %.1f", omsDelayTimer, icnctDelayTimer, tSRBSep, ATO_OMS_DT, MANUAL_OMS_DT );
		else sprintf_s( cbuf, 256, "%.2f %.2f %.3f", omsDelayTimer, icnctDelayTimer, tSRBSep );
		oapiWriteScenario_string( scn, "TIMERS", cbuf );
		sprintf_s( cbuf, 256, "%.3f %.3f %.4f %.3f", ReadCOMPOOL_SS( SCP_OMS_DT_COMPUTED ), ReadCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME ), ReadCOMPOOL_SS( SCP_OMS_TIME_SCALE_FACTOR ), ReadCOMPOOL_SS( SCP_MANUAL_OMS_DT ) );
		oapiWriteScenario_string( scn, "DUMP", cbuf );
		sprintf_s( cbuf, 256, "%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu", ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA ), ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD ), ReadCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD ),
			ReadCOMPOOL_IS( SCP_MODE2_IND ), ReadCOMPOOL_IS( SCP_OVRD_DUMP_ARM ), ReadCOMPOOL_IS( SCP_ORBITER_DUMP_ENA ), ReadCOMPOOL_IS( SCP_ORBITER_DUMP_INH ),
			ReadCOMPOOL_IS( SCP_ABORT_OMS_IGN_CMD ), ReadCOMPOOL_IS( SCP_ABORT_RCS_PX_CMD ), ReadCOMPOOL_IS( SCP_ABORT_RCS_NULL_CMD ) );
		oapiWriteScenario_string( scn, "CMDS", cbuf );
		return;
	}
}
