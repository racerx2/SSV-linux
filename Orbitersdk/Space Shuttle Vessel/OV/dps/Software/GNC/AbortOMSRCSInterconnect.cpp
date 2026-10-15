/****************************************************************************
  This file is part of Space Shuttle Vessel

  Abort OMS/RCS Interconnect (FSSR 4.184) implementation


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
#include "AbortOMSRCSInterconnect.h"
#include <DiscreteBundleManager.h>


namespace dps
{
	constexpr double IC_CYCLE = 0.08;// sequence pass [s]
	constexpr double IC_VLV_DELAY = 1.5;// time allowed for a valve response [s]
	constexpr int IC_MONITOR_PASSES = 3;// passes without a tank isolation valve closed indication before reconfiguration


	AbortOMSRCSInterconnect::AbortOMSRCSInterconnect( SimpleGPCSystem* _gpc ):SimpleGPCSoftware( _gpc, "AbortOMSRCSInterconnect" ),
		step( IDLE ), stepTimer( 0.0 ), stepFirstPass( true ), icInProgress( false ), rtnInProgress( false ), monitor( false ), xfdIsoFail( false ), bOXA( false ), icFail( 0 ), tCycle( 0.0 )
	{
		for (int s = 0; s < 2; s++) tkPosCount[s][0] = tkPosCount[s][1] = 0;
		for (int s = 0; s < 2; s++) cmdState[s] = 0;
		return;
	}

	AbortOMSRCSInterconnect::~AbortOMSRCSInterconnect( void )
	{
		return;
	}

	void AbortOMSRCSInterconnect::Realize( void )
	{
		const char* cmdname[2] = {"OMSRCS_VLV_L_CMD", "OMSRCS_VLV_R_CMD"};
		const char* indname[2] = {"OMSRCS_VLV_L_IND", "OMSRCS_VLV_R_IND"};
		for (unsigned int s = 0; s < 2; s++)
		{
			DiscreteBundle* pCmd = BundleManager()->CreateBundle( cmdname[s], 16 );
			DiscreteBundle* pInd = BundleManager()->CreateBundle( indname[s], 16 );
			for (unsigned int v = 0; v < VLV_COUNT; v++)
			{
				cmdOpen[s][v].Connect( pCmd, v );
				cmdClose[s][v].Connect( pCmd, v + VLV_COUNT );
				indOpen[s][v].Connect( pInd, v );
				indClosed[s][v].Connect( pInd, v + VLV_COUNT );
			}

			// held valve commands from the scenario
			for (unsigned int v = 0; v < VLV_COUNT; v++)
			{
				if (cmdState[s] & (1 << v)) cmdOpen[s][v].SetLine();
				if (cmdState[s] & (1 << (v + VLV_COUNT))) cmdClose[s][v].SetLine();
			}
		}
		return;
	}

	bool AbortOMSRCSInterconnect::OnMajorModeChange( unsigned int newMajorMode )
	{
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

	void AbortOMSRCSInterconnect::OnPreStep( double simt, double simdt, double mjd )
	{
		tCycle += simdt;
		if (tCycle < IC_CYCLE) return;
		double dt = tCycle;
		tCycle = 0.0;
		Cycle( dt );
		return;
	}

	void AbortOMSRCSInterconnect::TerminateAll( void )
	{
		for (unsigned int s = 0; s < 2; s++)
		{
			for (unsigned int v = 0; v < VLV_COUNT; v++)
			{
				cmdOpen[s][v].ResetLine();
				cmdClose[s][v].ResetLine();
			}
			cmdState[s] = 0;
		}
		return;
	}

	void AbortOMSRCSInterconnect::Issue( unsigned int side, VLV v, bool open )
	{
		if (open)
		{
			cmdOpen[side][v].SetLine();
			cmdState[side] |= (1 << v);
		}
		else
		{
			cmdClose[side][v].SetLine();
			cmdState[side] |= (1 << (v + VLV_COUNT));
		}
		return;
	}

	void AbortOMSRCSInterconnect::Terminate( unsigned int side, VLV v, bool open )
	{
		if (open)
		{
			cmdOpen[side][v].ResetLine();
			cmdState[side] &= ~(1 << v);
		}
		else
		{
			cmdClose[side][v].ResetLine();
			cmdState[side] &= ~(1 << (v + VLV_COUNT));
		}
		return;
	}

	bool AbortOMSRCSInterconnect::Open( unsigned int side, VLV v ) const
	{
		return indOpen[side][v].IsSet();
	}

	bool AbortOMSRCSInterconnect::Closed( unsigned int side, VLV v ) const
	{
		return indClosed[side][v].IsSet();
	}

	void AbortOMSRCSInterconnect::Goto( STEP s )
	{
		step = s;
		stepTimer = 0.0;
		stepFirstPass = true;
		return;
	}

	void AbortOMSRCSInterconnect::SetAllJetInh( bool inh )
	{
		WriteCOMPOOL_IS( SCP_RCS_12_ALL_JET_INH, inh ? 1 : 0 );
		WriteCOMPOOL_IS( SCP_RCS_345_ALL_JET_INH, inh ? 1 : 0 );
		return;
	}

	void AbortOMSRCSInterconnect::Step16( void )
	{
		SetAllJetInh( false );
		WriteCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH, 0 );
		WriteCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH, 0 );
		WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT, 0 );
		rtnInProgress = false;
		step = IDLE;
		return;
	}

	bool AbortOMSRCSInterconnect::Monitor( void )
	{
		// steps 7 to 8A: a tank isolation valve that loses its closed indication sends that manifold group back to its RCS tanks
		for (unsigned int g = 0; g < 2; g++)
		{
			for (unsigned int s = 0; s < 2; s++)
			{
				if (Closed( s, (g == 0) ? TK12 : TK345 )) tkPosCount[s][g] = 0;
				else tkPosCount[s][g]++;

				if (tkPosCount[s][g] >= IC_MONITOR_PASSES)
				{
					monitor = false;
					if (g == 0)
					{
						xfdIsoFail = true;
						WriteCOMPOOL_IS( SCP_RCS_12_ALL_JET_INH, 1 );
					}
					else WriteCOMPOOL_IS( SCP_RCS_345_ALL_JET_INH, 1 );
					icInProgress = true;
					icFail++;
					Goto( IC_RECONFIG_XFD );
					return true;
				}
			}
		}
		return false;
	}

	void AbortOMSRCSInterconnect::Cycle( double dt )
	{
		stepTimer += dt;

		// step 1
		if (!icInProgress && !rtnInProgress)
		{
			bool icCmd = (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD ) != 0);
			bool rtnCmd = (ReadCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD ) != 0);
			bool cplt = (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ) != 0);

			if (icCmd && !cplt)
			{
				TerminateAll();
				WriteCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH, 0 );
				WriteCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH, 0 );
				monitor = false;
				xfdIsoFail = false;
				SetAllJetInh( true );
				icInProgress = true;
				icFail = 0;
				for (int s = 0; s < 2; s++) tkPosCount[s][0] = tkPosCount[s][1] = 0;
				Goto( IC_TK_CLOSE );
			}
			else if (rtnCmd && cplt)
			{
				TerminateAll();
				monitor = false;
				xfdIsoFail = false;
				rtnInProgress = true;
				if (ReadCOMPOOL_IS( SCP_MODE2_IND ) != 0) Goto( M2_TK_OPEN );
				else
				{
					SetAllJetInh( true );
					Goto( RTN_OMS_XFD_CLOSE );
				}
			}
			else if (cplt && monitor)
			{
				if (!Monitor()) return;
			}
			else return;
		}

		// the steps run in one pass until one waits for its valves
		bool wait = false;
		while (!wait && (step != IDLE))
		{
			bool first = stepFirstPass;
			stepFirstPass = false;

			switch (step)
			{
				case IC_TK_CLOSE:// step 2
					if (first)
					{
						for (unsigned int s = 0; s < 2; s++)
						{
							Issue( s, TK12, false );
							Issue( s, TK345, false );
						}
					}
					if (stepTimer >= IC_VLV_DELAY) Goto( IC_XFD_OPEN );
					else wait = true;
					break;
				case IC_XFD_OPEN:// step 3
					if (first)
					{
						if (!(Closed( 0, TK12 ) && Closed( 1, TK12 )) || Open( 0, TK12 ) || Open( 1, TK12 ))
						{
							WriteCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH, 1 );
							icFail++;
						}
						else
						{
							Issue( 0, XFD12, true );
							Issue( 1, XFD12, true );
						}

						if (!(Closed( 0, TK345 ) && Closed( 1, TK345 )))
						{
							WriteCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH, 1 );
							icFail++;
						}
						else
						{
							Issue( 0, XFD345, true );
							Issue( 1, XFD345, true );
						}

						if (icFail > 1)
						{
							Goto( IC_RECONFIG_TK );
							break;
						}
						Issue( 0, OXB, true );
						Issue( 1, OXB, true );
					}
					if (stepTimer >= IC_VLV_DELAY) Goto( IC_XFD_CHECK );
					else wait = true;
					break;
				case IC_XFD_CHECK:// step 4
					if ((ReadCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH ) == 0) && !Open( 0, XFD12 ) && !Open( 1, XFD12 ))
					{
						icFail++;
						xfdIsoFail = true;
					}
					if ((ReadCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH ) == 0) && !Open( 0, XFD345 ) && !Open( 1, XFD345 )) icFail++;

					if (icFail == 0) Goto( IC_OMS_XFD_A );
					else Goto( IC_RECONFIG_XFD );
					break;
				case IC_RECONFIG_XFD:// step 5
					if (first)
					{
						if (icFail > 1)
						{
							for (unsigned int s = 0; s < 2; s++)
							{
								Terminate( s, XFD12, true );
								Terminate( s, XFD345, true );
								Terminate( s, OXB, true );
								Issue( s, XFD12, false );
								Issue( s, XFD345, false );
								Issue( s, OXB, false );
							}
						}
						else if ((ReadCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH ) != 0) || (ReadCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH ) != 0))
						{
							Goto( IC_RECONFIG_TK );
							break;
						}
						else if (xfdIsoFail)
						{
							for (unsigned int s = 0; s < 2; s++)
							{
								Terminate( s, XFD12, true );
								Issue( s, XFD12, false );
							}
							WriteCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH, 1 );
						}
						else
						{
							for (unsigned int s = 0; s < 2; s++)
							{
								Terminate( s, XFD345, true );
								Issue( s, XFD345, false );
							}
							WriteCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH, 1 );
						}
					}
					if (stepTimer >= IC_VLV_DELAY) Goto( IC_RECONFIG_TK );
					else wait = true;
					break;
				case IC_RECONFIG_TK:// step 6, interconnect failure
					if (first)
					{
						bool g12 = (icFail > 1) || (ReadCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH ) != 0);
						bool g345 = (icFail > 1) || !g12;
						for (unsigned int s = 0; s < 2; s++)
						{
							if (g12)
							{
								Terminate( s, TK12, false );
								Issue( s, TK12, true );
							}
							if (g345)
							{
								Terminate( s, TK345, false );
								Issue( s, TK345, true );
							}
						}
					}
					if (stepTimer >= IC_VLV_DELAY) Goto( IC_OMS_XFD_A );
					else wait = true;
					break;
				case IC_OMS_XFD_A:// step 11
					if (icFail > 1)
					{
						WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 0 );
						WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 0 );
						WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 1 );
						icInProgress = false;
						Step16();
						break;
					}
					if (first)
					{
						bool cplt = (ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ) != 0);
						bOXA = icInProgress && !cplt && (!Open( 0, OXB ) || !Open( 1, OXB ));
						if (bOXA)
						{
							Issue( 0, OXA, true );
							Issue( 1, OXA, true );
						}
					}
					if (bOXA)
					{
						if (stepTimer < IC_VLV_DELAY)
						{
							wait = true;
							break;
						}
						if (!Open( 0, OXA ) || !Open( 1, OXA ))
						{
							// no OMS crossfeed path: complete, and let the return to normal run
							WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_ENA, 0 );
							WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CMD, 0 );
							WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT, 1 );
							WriteCOMPOOL_IS( SCP_OMS_RCS_RTN_NORM_CMD, 1 );
							icInProgress = false;
							step = IDLE;
							break;
						}
					}
					WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT, 1 );
					SetAllJetInh( false );
					if ((icFail == 0) && icInProgress)
					{
						icInProgress = false;
						monitor = true;
						step = IDLE;
						if (Monitor()) break;
					}
					else
					{
						icInProgress = false;
						step = IDLE;
					}
					break;
				case RTN_OMS_XFD_CLOSE:// step 9
					if (first)
					{
						for (unsigned int s = 0; s < 2; s++)
						{
							Issue( s, OXA, false );
							Issue( s, OXB, false );
						}
					}
					if (stepTimer >= IC_VLV_DELAY) Goto( RTN_TK_OPEN );
					else wait = true;
					break;
				case RTN_TK_OPEN:// step 6, return to normal
				case M2_TK_OPEN:// step 12
					if (first)
					{
						for (unsigned int s = 0; s < 2; s++)
						{
							Issue( s, TK12, true );
							Issue( s, TK345, true );
						}
					}
					if (stepTimer >= IC_VLV_DELAY) Goto( (step == RTN_TK_OPEN) ? RTN_XFD : M2_XFD );
					else wait = true;
					break;
				case RTN_XFD:// step 10
				case M2_XFD:// steps 13 and 14
					if (first)
					{
						// a 1/2 tank isolation valve that did not open keeps its crossfeed open, so the manifold stays fed from the 3/4/5 tanks
						for (unsigned int s = 0; s < 2; s++)
						{
							bool tkOpen = Open( s, TK12 ) && !Closed( s, TK12 );
							Issue( s, XFD12, !tkOpen );
							Issue( s, XFD345, !tkOpen );
						}
						if (step == RTN_XFD) SetAllJetInh( false );
					}
					if (stepTimer >= IC_VLV_DELAY)
					{
						if (step == RTN_XFD) Step16();
						else Goto( M2_OMS_XFD_CLOSE );
					}
					else wait = true;
					break;
				case M2_OMS_XFD_CLOSE:// step 15
					if (first)
					{
						for (unsigned int s = 0; s < 2; s++)
						{
							Issue( s, OXA, false );
							Issue( s, OXB, false );
						}
					}
					if (stepTimer >= IC_VLV_DELAY) Step16();
					else wait = true;
					break;
				default:
					step = IDLE;
					break;
			}
		}
		return;
	}

	bool AbortOMSRCSInterconnect::OnParseLine( const char* keyword, const char* value )
	{
		if (!_strnicmp( keyword, "SEQ", 3 ))
		{
			int st = 0, fp = 1, ic = 0, rt = 0, mo = 0, xf = 0, fl = 0, oxa = 0;
			double tmr = 0.0;
			sscanf_s( value, "%d %lf %d %d %d %d %d %d %d", &st, &tmr, &fp, &ic, &rt, &mo, &xf, &fl, &oxa );
			step = static_cast<STEP>(st);
			stepTimer = tmr;
			stepFirstPass = (fp != 0);
			icInProgress = (ic != 0);
			rtnInProgress = (rt != 0);
			monitor = (mo != 0);
			xfdIsoFail = (xf != 0);
			icFail = fl;
			bOXA = (oxa != 0);
			return true;
		}
		else if (!_strnicmp( keyword, "FLAGS", 5 ))
		{
			int cp = 0, m12 = 0, m345 = 0, a12 = 0, a345 = 0;
			sscanf_s( value, "%d %d %d %d %d", &cp, &m12, &m345, &a12, &a345 );
			WriteCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT, cp );
			WriteCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH, m12 );
			WriteCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH, m345 );
			WriteCOMPOOL_IS( SCP_RCS_12_ALL_JET_INH, a12 );
			WriteCOMPOOL_IS( SCP_RCS_345_ALL_JET_INH, a345 );
			return true;
		}
		else if (!_strnicmp( keyword, "CMD", 3 ))
		{
			sscanf_s( value, "%u %u", &cmdState[0], &cmdState[1] );
			return true;
		}
		return false;
	}

	void AbortOMSRCSInterconnect::OnSaveState( FILEHANDLE scn ) const
	{
		char cbuf[128];
		sprintf_s( cbuf, 128, "%d %.2f %d %d %d %d %d %d %d", static_cast<int>(step), stepTimer, stepFirstPass, icInProgress, rtnInProgress, monitor, xfdIsoFail, icFail, bOXA );
		oapiWriteScenario_string( scn, "SEQ", cbuf );
		sprintf_s( cbuf, 128, "%hu %hu %hu %hu %hu", ReadCOMPOOL_IS( SCP_OMS_RCS_ICNCT_CPLT ), ReadCOMPOOL_IS( SCP_AFT_MANF_12_JET_INH ), ReadCOMPOOL_IS( SCP_AFT_MANF_345_JET_INH ),
			ReadCOMPOOL_IS( SCP_RCS_12_ALL_JET_INH ), ReadCOMPOOL_IS( SCP_RCS_345_ALL_JET_INH ) );
		oapiWriteScenario_string( scn, "FLAGS", cbuf );
		sprintf_s( cbuf, 128, "%u %u", cmdState[0], cmdState[1] );
		oapiWriteScenario_string( scn, "CMD", cbuf );
		return;
	}
}
