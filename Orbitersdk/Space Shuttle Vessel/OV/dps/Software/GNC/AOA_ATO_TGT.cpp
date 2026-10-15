/****************************************************************************
  This file is part of Space Shuttle Vessel

  AOA/ATO Targeting (FSSR 4.5) implementation


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
#include "AOA_ATO_TGT.h"
#include "AscentDAP.h"
#include "OMSBurnSoftware.h"
#include "StateVectorSoftware.h"
#include "GNCUtilities.h"
#include "../../../Atlantis.h"
#include <MathSSV.h>
#include <EngConst.h>
#include <cassert>


namespace dps
{
	// K-loads and constants
	constexpr double FT_OMS = 6.087e3;// one OMS engine [lbf]
	constexpr double VEX_OMS = 1.01368e4;// OMS exhaust velocity [ft/s]
	constexpr double MDOT_OMS = FT_OMS / VEX_OMS;// one OMS engine [slug/s]
	constexpr double H_EI = 400000.0;// entry interface above the ellipsoid [ft]
	constexpr double ORB_RATE = 1.185e-3;// circular orbit rate [rad/s]
	constexpr double DT_AOA_PRED = 300.0;// prediction step [s]
	constexpr int NMAX_DIP = 10;// maximum PEG passes

	static const double NM2FT = 1.0 / NAUTMI_PER_FT;


	AOA_ATO_TGT::AOA_ATO_TGT( SimpleGPCSystem* _gpc ):SimpleGPCSoftware( _gpc, "AOA_ATO_TGT" ),
		pAscentDAP( NULL ), pOMSBurnSoftware( NULL ), pStateVector( NULL ), pGNCUtilities( NULL ),
		scheduled( false ), atoMECO( false ), oms1Index( 0 ), massOrbSet( false ), massOMSInitSet( false ),
		pegBusy( false ), aoaI( 0 ), aoaIncl( 0.0 ), aoaTheta( 0.0 ), aoaM( 0.0 ), tigOPS3( 0.0 ), massOMSPreOMS2( 0.0 ), RGD( _V( 0, 0, 0 ) ), VGD( _V( 0, 0, 0 ) )
	{
		// I-load defaults. ATO MECO targets equal the nominal ones (JSC-10559 2.3.1), so V_ATO_SW = 0.
		V_ATO_SW = 0.0;
		bATOMECOILoad = false;
		RD_ATO = 0.0;
		VD_ATO = 0.0;
		GAMD_ATO = 0.0;
		S_IY = 0;
		IY_MIN_EF = _V( 0.0, 0.0, -1.0 );
		V_MSSN_CNTN = 16000.0;
		V_LIN = 12000.0;
		V_ZERO = 16000.0;
		EPS_K2 = 0.05;
		// masses from the vehicle unless I-loaded: OMS load at liftoff, orbiter mass at ET separation plus the OMS burned pre-MECO
		MASS_ORB = 0.0;
		MASS_OMS_INIT = 0.0;
		MASS_CG_CNTL = 0.0;
		DV_RQD = 550.0;
		DV_TOL = 0.0;
		SFUELD = 0;
		THETA_LS_REF = 0.0;// not loaded: no OMS-2 TIG adjustment

		// STS-75 direct insertion targets (JSC-10559 table 4.3.4.6.2-I); theta from the launch site
		HTGT_ATO_OMS1 = 114.0 * NM2FT;
		THETA_ATO_OMS1 = 235.0 * RAD;
		C1_ATO_OMS1 = 0.0;
		C2_ATO_OMS1 = 0.0;
		DTIG_ATO_OMS1 = 108.0;

		const double atoOMS2[2][3] = {{113.0, 415.0, 3180.0}, {93.0, 415.0, 3180.0}};// ATO, MIN HP: HT [nm], theta [deg], DTIG [s]
		for (int i = 0; i < 2; i++)
		{
			HTGT_ATO_OMS2[i] = atoOMS2[i][0] * NM2FT;
			THETA_ATO_OMS2[i] = atoOMS2[i][1] * RAD;
			C1_ATO_OMS2[i] = 0.0;
			C2_ATO_OMS2[i] = 0.0;
			DTIG_ATO_OMS2[i] = atoOMS2[i][2];
		}

		const double aoaOMS1[3][3] = {{146.0, 170.0, 228.0}, {146.0, 170.0, 228.0}, {95.0, 225.0, 108.0}};// OPT-AOA, OPT-AOA, AOA-SH
		for (int i = 0; i < 3; i++)
		{
			HTGT_AOA_OMS1[i] = aoaOMS1[i][0] * NM2FT;
			THETA_AOA_OMS1[i] = aoaOMS1[i][1] * RAD;
			C1_AOA_OMS1[i] = 0.0;
			C2_AOA_OMS1[i] = 0.0;
			DTIG_AOA_OMS1[i] = aoaOMS1[i][2];
		}

		// AOA OMS-2 (EI) targets, constant terms: DTIG [s], C1 [ft/s], C2, theta [deg] of the entry interface from the launch site.
		// The STS-75 table gives theta from TIG; the TIG downrange angle is added for an 88.5 min orbit and 22 deg at ET separation.
		const double aoaOMS2[10][4] = {
			{1782.0, 16012.0, -0.6356, 133.6},// 1 (I_TGT 3) shallow, after OPT-AOA OMS-1
			{2741.0, 15657.0, -0.6284, 73.1},// 2 (4) steep, after OPT-AOA OMS-1
			{2342.0, 19072.0, -0.7529, 83.5},// 3 (5) shallow, after shallow OMS-1
			{2501.0, 16625.0, -0.6658, 86.1},// 4 (6) steep, after ATO OMS-1
			{1893.0, 16983.0, -0.6726, 121.4},// 5 (7) shallow, after ATO OMS-1
			{1782.0, 16012.0, -0.6356, 133.6},// 6 (8) shallow default after OMS-1
			{787.0, 14821.0, -0.5905, 204.7},// 7 (9) shallow, no OMS-1, MECO apogee 150-95
			{2000.0, 14843.0, -0.5968, 125.4},// 8 (10) steep, no OMS-1, MECO apogee 200-150
			{787.0, 15580.0, -0.6201, 205.0},// 9 (11) shallow, no OMS-1, MECO apogee 200-150
			{1518.0, 15620.0, -0.6269, 159.6}};// 10 (12) steep, after a nominal OMS-1 or none
		for (int i = 0; i < 10; i++)
		{
			double tigLS = 22.0 + (360.0 * aoaOMS2[i][0] / 5310.0);
			for (int k = 0; k < 3; k++)
			{
				DTIG_AOA_OMS2[(i * 3) + k] = (k == 0) ? aoaOMS2[i][0] : 0.0;
				C1_AOA_OMS2[(i * 3) + k] = (k == 0) ? aoaOMS2[i][1] : 0.0;
				C2_AOA_OMS2[(i * 3) + k] = (k == 0) ? aoaOMS2[i][2] : 0.0;
				THETA_AOA_OMS2[(i * 3) + k] = (k == 0) ? ((aoaOMS2[i][3] + tigLS) * RAD) : 0.0;
			}
		}
		return;
	}

	AOA_ATO_TGT::~AOA_ATO_TGT( void )
	{
		return;
	}

	void AOA_ATO_TGT::Realize( void )
	{
		pAscentDAP = dynamic_cast<AscentDAP*>(FindSoftware( "AscentDAP" ));
		assert( (pAscentDAP != NULL) && "AOA_ATO_TGT::Realize.pAscentDAP" );
		pOMSBurnSoftware = dynamic_cast<OMSBurnSoftware*>(FindSoftware( "OMSBurnSoftware" ));
		assert( (pOMSBurnSoftware != NULL) && "AOA_ATO_TGT::Realize.pOMSBurnSoftware" );
		pStateVector = dynamic_cast<StateVectorSoftware*>(FindSoftware( "StateVectorSoftware" ));
		assert( (pStateVector != NULL) && "AOA_ATO_TGT::Realize.pStateVector" );
		pGNCUtilities = dynamic_cast<GNCUtilities*>(FindSoftware( "GNCUtilities" ));
		assert( (pGNCUtilities != NULL) && "AOA_ATO_TGT::Realize.pGNCUtilities" );

		peg4.SetPlanetParameters( EARTH_MU );
		return;
	}

	void AOA_ATO_TGT::ReadILOADs( const std::map<std::string,std::string>& ILOADs )
	{
		if (HasILOAD( "V_ATO_SW", ILOADs )) GetValILOAD( "V_ATO_SW", ILOADs, V_ATO_SW );
		if (HasILOAD( "VD_ATO", ILOADs ) && HasILOAD( "RD_ATO", ILOADs ) && HasILOAD( "GAMD_ATO", ILOADs ))
		{
			GetValILOAD( "RD_ATO", ILOADs, RD_ATO );
			GetValILOAD( "VD_ATO", ILOADs, VD_ATO );
			GetValILOAD( "GAMD_ATO", ILOADs, GAMD_ATO );
			bATOMECOILoad = true;
		}
		if (HasILOAD( "S_IY", ILOADs )) GetValILOAD( "S_IY", ILOADs, S_IY );
		if (HasILOAD( "IY_MIN_EF", ILOADs )) GetValILOAD( "IY_MIN_EF", ILOADs, 3, IY_MIN_EF.data );
		if (HasILOAD( "V_MSSN_CNTN", ILOADs )) GetValILOAD( "V_MSSN_CNTN", ILOADs, V_MSSN_CNTN );
		if (HasILOAD( "V_LIN", ILOADs )) GetValILOAD( "V_LIN", ILOADs, V_LIN );
		if (HasILOAD( "V_ZERO", ILOADs )) GetValILOAD( "V_ZERO", ILOADs, V_ZERO );
		if (HasILOAD( "EPS_K2", ILOADs )) GetValILOAD( "EPS_K2", ILOADs, EPS_K2 );
		if (HasILOAD( "MASS_ORB", ILOADs ))
		{
			GetValILOAD( "MASS_ORB", ILOADs, MASS_ORB );
			massOrbSet = true;
		}
		if (HasILOAD( "MASS_OMS_INIT", ILOADs ))
		{
			GetValILOAD( "MASS_OMS_INIT", ILOADs, MASS_OMS_INIT );
			massOMSInitSet = true;
		}
		if (HasILOAD( "MASS_CG_CNTL", ILOADs )) GetValILOAD( "MASS_CG_CNTL", ILOADs, MASS_CG_CNTL );
		if (HasILOAD( "DV_RQD", ILOADs )) GetValILOAD( "DV_RQD", ILOADs, DV_RQD );
		if (HasILOAD( "DV_TOL", ILOADs )) GetValILOAD( "DV_TOL", ILOADs, DV_TOL );
		if (HasILOAD( "SFUELD", ILOADs )) GetValILOAD( "SFUELD", ILOADs, SFUELD );
		if (HasILOAD( "THETA_LS_REF", ILOADs )) GetValILOAD( "THETA_LS_REF", ILOADs, THETA_LS_REF );

		if (HasILOAD( "HTGT_ATO_OMS1", ILOADs )) GetValILOAD( "HTGT_ATO_OMS1", ILOADs, HTGT_ATO_OMS1 );
		if (HasILOAD( "THETA_ATO_OMS1", ILOADs )) GetValILOAD( "THETA_ATO_OMS1", ILOADs, THETA_ATO_OMS1 );
		if (HasILOAD( "C1_ATO_OMS1", ILOADs )) GetValILOAD( "C1_ATO_OMS1", ILOADs, C1_ATO_OMS1 );
		if (HasILOAD( "C2_ATO_OMS1", ILOADs )) GetValILOAD( "C2_ATO_OMS1", ILOADs, C2_ATO_OMS1 );
		if (HasILOAD( "DTIG_ATO_OMS1", ILOADs )) GetValILOAD( "DTIG_ATO_OMS1", ILOADs, DTIG_ATO_OMS1 );
		if (HasILOAD( "HTGT_ATO_OMS2", ILOADs )) GetValILOAD( "HTGT_ATO_OMS2", ILOADs, 2, HTGT_ATO_OMS2 );
		if (HasILOAD( "THETA_ATO_OMS2", ILOADs )) GetValILOAD( "THETA_ATO_OMS2", ILOADs, 2, THETA_ATO_OMS2 );
		if (HasILOAD( "C1_ATO_OMS2", ILOADs )) GetValILOAD( "C1_ATO_OMS2", ILOADs, 2, C1_ATO_OMS2 );
		if (HasILOAD( "C2_ATO_OMS2", ILOADs )) GetValILOAD( "C2_ATO_OMS2", ILOADs, 2, C2_ATO_OMS2 );
		if (HasILOAD( "DTIG_ATO_OMS2", ILOADs )) GetValILOAD( "DTIG_ATO_OMS2", ILOADs, 2, DTIG_ATO_OMS2 );
		if (HasILOAD( "HTGT_AOA_OMS1", ILOADs )) GetValILOAD( "HTGT_AOA_OMS1", ILOADs, 3, HTGT_AOA_OMS1 );
		if (HasILOAD( "THETA_AOA_OMS1", ILOADs )) GetValILOAD( "THETA_AOA_OMS1", ILOADs, 3, THETA_AOA_OMS1 );
		if (HasILOAD( "C1_AOA_OMS1", ILOADs )) GetValILOAD( "C1_AOA_OMS1", ILOADs, 3, C1_AOA_OMS1 );
		if (HasILOAD( "C2_AOA_OMS1", ILOADs )) GetValILOAD( "C2_AOA_OMS1", ILOADs, 3, C2_AOA_OMS1 );
		if (HasILOAD( "DTIG_AOA_OMS1", ILOADs )) GetValILOAD( "DTIG_AOA_OMS1", ILOADs, 3, DTIG_AOA_OMS1 );
		if (HasILOAD( "THETA_AOA_OMS2", ILOADs )) GetValILOAD( "THETA_AOA_OMS2", ILOADs, 30, THETA_AOA_OMS2 );
		if (HasILOAD( "C1_AOA_OMS2", ILOADs )) GetValILOAD( "C1_AOA_OMS2", ILOADs, 30, C1_AOA_OMS2 );
		if (HasILOAD( "C2_AOA_OMS2", ILOADs )) GetValILOAD( "C2_AOA_OMS2", ILOADs, 30, C2_AOA_OMS2 );
		if (HasILOAD( "DTIG_AOA_OMS2", ILOADs )) GetValILOAD( "DTIG_AOA_OMS2", ILOADs, 30, DTIG_AOA_OMS2 );
		return;
	}

	bool AOA_ATO_TGT::OnMajorModeChange( unsigned int newMajorMode )
	{
		if ((newMajorMode == 102) && (GetMajorMode() == 101) && !massOMSInitSet)
		{
			MASS_OMS_INIT = STS()->GetOMSPropellantMass() * KG2LBM / G_2_FPS2;
			massOMSInitSet = true;
		}
		else if ((newMajorMode == 104) && (GetMajorMode() == 103) && !massOrbSet)
		{
			MASS_ORB = VehicleMass() + DM_PREMECO();
			massOrbSet = true;
		}

		pegBusy = false;
		return ((newMajorMode >= 101) && (newMajorMode <= 106));
	}

	void AOA_ATO_TGT::OnPreStep( double simt, double simdt, double mjd )
	{
		if (pegBusy)
		{
			if (peg4.PEG_TSK())
			{
				pegBusy = false;
				AOA_OMS2_Finish( length( peg4.GetDeltaV() ) );
			}
			return;
		}

		if (scheduled)
		{
			scheduled = false;
			Sequence();
		}
		return;
	}

	void AOA_ATO_TGT::Sequence( void )
	{
		unsigned int mm = GetMajorMode();
		bool aoa = (ReadCOMPOOL_IS( SCP_S_AOA ) != 0);
		bool ato = (ReadCOMPOOL_IS( SCP_S_ATO ) != 0);

		if (mm == 103)
		{
			if (pAscentDAP->GetFineCount()) return;
			ATO_MECO_TGT_SEL();
			ATO_OMS_PREMECO_BRN_DET();
		}
		else if (mm == 104)
		{
			// OMS-1 targets only before the OMS-1 execute command
			if (pOMSBurnSoftware->BurnEnabled()) return;
			if (aoa) AOA_ONORB_OMS1();
			else if (ato) ATO_ONORB_OMS1();
		}
		else if (mm == 105)
		{
			if (pOMSBurnSoftware->BurnEnabled()) return;
			if (aoa) AOA_ONORB_OMS2();
			else if (ato) ATO_ONORB_OMS2();
		}
		return;
	}

	void AOA_ATO_TGT::CurrentState( VECTOR3& R, VECTOR3& V, double& T )
	{
		// the navigated state, M50 [ft, ft/s]; the user parameter state (R_AVGG) runs only from MM 104
		pStateVector->GetCurrentStateVectorsM50( R, V );
		R *= MPS2FPS;
		V *= MPS2FPS;
		T = ReadClock();
		return;
	}

	void AOA_ATO_TGT::ATO_MECO_TGT_SEL( void )
	{
		VECTOR3 R, V;
		double T;
		CurrentState( R, V, T );

		// V_ME_OUT is the velocity at SSME failure, or the current relative velocity if all three run
		double vmeout;
		if (pAscentDAP->GetNSSME() == 3)
		{
			vmeout = length( pGNCUtilities->V_REL( V, R ) );
			WriteCOMPOOL_SS( SCP_V_ME_OUT, static_cast<float>(vmeout) );
		}
		else vmeout = ReadCOMPOOL_SS( SCP_V_ME_OUT );

		double rd, vd, gamd;
		pAscentDAP->GetMECOTargets( rd, vd, gamd );
		atoMECO = false;
		if (vmeout < V_ATO_SW)
		{
			atoMECO = true;
			if (bATOMECOILoad)
			{
				rd = RD_ATO * FPS2MS;
				vd = VD_ATO * FPS2MS;
				gamd = GAMD_ATO;
			}
		}

		// variable IY: the current plane, not below the minimum inclination
		VECTOR3 IY;
		bool newIY = false;
		if ((S_IY != 0) && (vmeout < V_MSSN_CNTN))
		{
			IY = unit( crossp( V, R ) );
			VECTOR3 IY_MIN_M50 = unit( mul( pGNCUtilities->EARTH_FIXED_TO_M50_COORD( ReadCOMPOOL_SD( SCP_T_MET_REF ) ), IY_MIN_EF ) );
			if (IY.z < IY_MIN_M50.z) IY = IY_MIN_M50;
			newIY = true;
			WriteCOMPOOL_IS( SCP_S_ATO_IY, 1 );
		}

		pAscentDAP->SetMECOTargets( rd, vd, gamd, newIY ? &IY : NULL );
		oapiWriteLogV( "(SSV_OV) [INFO] ATO MECO target selection: V_ME_OUT %.0f fps, %s MECO targets%s", vmeout, atoMECO ? "ATO" : "nominal", newIY ? ", variable IY" : "" );
		return;
	}

	void AOA_ATO_TGT::ATO_OMS_PREMECO_BRN_DET( void )
	{
		double vmeout = ReadCOMPOOL_SS( SCP_V_ME_OUT );
		double K2;
		if (vmeout <= V_LIN) K2 = 1.0;
		else if (vmeout <= V_ZERO) K2 = (V_ZERO - vmeout) / (V_ZERO - V_LIN);
		else K2 = 0.0;
		if (K2 < EPS_K2) K2 = 0.0;

		WriteCOMPOOL_SS( SCP_K2, static_cast<float>(K2) );
		WriteCOMPOOL_IS( SCP_S_ABORT_CONTROL, 1 );
		oapiWriteLogV( "(SSV_OV) [INFO] ATO pre-MECO OMS burn scale factor K2 %.3f", K2 );
		return;
	}

	void AOA_ATO_TGT::EnsureMasses( void )
	{
		// scenarios saved before liftoff or ET separation was captured
		if (!massOMSInitSet)
		{
			MASS_OMS_INIT = (STS()->GetOMSPropellantMass() * KG2LBM / G_2_FPS2) + DM_PREMECO();
			massOMSInitSet = true;
		}
		if (!massOrbSet)
		{
			MASS_ORB = VehicleMass() + DM_PREMECO();
			massOrbSet = true;
		}
		return;
	}

	double AOA_ATO_TGT::VehicleMass( void ) const
	{
		return STS()->GetMass() * KG2LBM / G_2_FPS2;
	}

	double AOA_ATO_TGT::DM_PREMECO( void ) const
	{
		// the OMS equivalent on time runs at the two OMS engine flow rate
		return ReadCOMPOOL_SS( SCP_OMS_EQUIV_ON_TIME ) * 2.0 * MDOT_OMS;
	}

	double AOA_ATO_TGT::DV_OMS( double M, double massOMS ) const
	{
		if (massOMS <= 0.0) return 0.0;
		if (massOMS >= M) massOMS = 0.99 * M;
		return VEX_OMS * log( M / (M - massOMS) );
	}

	double AOA_ATO_TGT::AOAOMS2Param( const double* table, int I, double incl ) const
	{
		const double* c = table + ((I - 1) * 3);
		return c[0] + (c[1] * incl) + (c[2] * incl * incl);
	}

	void AOA_ATO_TGT::AOA_ONORB_OMS1( void )
	{
		EnsureMasses();

		int itgt;
		if (ReadCOMPOOL_IS( SCP_AOA_TGT_SEL ) != 0)
		{
			itgt = ReadCOMPOOL_IS( SCP_I_TGT );
			WriteCOMPOOL_IS( SCP_AOA_TGT_SEL, 0 );
		}
		else
		{
			// ATO MECO and shallow EI, or nominal MECO and nominal EI with the shallow default if the OMS delta V falls short
			itgt = atoMECO ? 3 : 4;
			if (itgt == 4)
			{
				double M = VehicleMass();
				double massPreOMS1 = MASS_OMS_INIT - DM_PREMECO() - MASS_CG_CNTL;
				double vdmag = pAscentDAP->GetMECOTargetVelocity() * MPS2FPS;
				VECTOR3 R, V;
				double T;
				CurrentState( R, V, T );
				double dvexcess = DV_OMS( M, massPreOMS1 ) - DV_RQD + length( V ) - vdmag + DV_TOL;
				oapiWriteLogV( "(SSV_OV) [INFO] AOA OMS-1 excess delta V %.0f fps", dvexcess );
				if (dvexcess < 0.0) itgt = 5;
			}
		}
		if ((itgt < 3) || (itgt > 5)) return;

		int i = itgt - 3;
		pOMSBurnSoftware->SetAbortTarget( HTGT_AOA_OMS1[i], THETA_AOA_OMS1[i], C1_AOA_OMS1[i], C2_AOA_OMS1[i], DTIG_AOA_OMS1[i], 0.0, false );
		WriteCOMPOOL_IS( SCP_I_TGT, itgt );
		oms1Index = itgt;
		WriteCOMPOOL_IS( SCP_ABORT_INIT, 1 );
		return;
	}

	void AOA_ATO_TGT::ATO_ONORB_OMS1( void )
	{
		if (ReadCOMPOOL_IS( SCP_ATO_TGT_SEL ) != 0) WriteCOMPOOL_IS( SCP_ATO_TGT_SEL, 0 );

		pOMSBurnSoftware->SetAbortTarget( HTGT_ATO_OMS1, THETA_ATO_OMS1, C1_ATO_OMS1, C2_ATO_OMS1, DTIG_ATO_OMS1, 0.0, false );
		WriteCOMPOOL_IS( SCP_I_TGT, 1 );
		oms1Index = 1;
		WriteCOMPOOL_IS( SCP_ABORT_INIT, 1 );
		return;
	}

	void AOA_ATO_TGT::ATO_ONORB_OMS2( void )
	{
		int itgt;
		if (ReadCOMPOOL_IS( SCP_ATO_TGT_SEL ) != 0)
		{
			itgt = ReadCOMPOOL_IS( SCP_I_TGT );
			WriteCOMPOOL_IS( SCP_ATO_TGT_SEL, 0 );
		}
		else itgt = (oms1Index == 1) ? 1 : 2;// after an ATO OMS-1, or after a nominal OMS-1 (or none)
		if ((itgt < 1) || (itgt > 2)) return;

		int i = itgt - 1;
		pOMSBurnSoftware->SetAbortTarget( HTGT_ATO_OMS2[i], THETA_ATO_OMS2[i], C1_ATO_OMS2[i], C2_ATO_OMS2[i], DTIG_ATO_OMS2[i], 0.0, false );
		WriteCOMPOOL_IS( SCP_I_TGT, itgt );
		WriteCOMPOOL_IS( SCP_ABORT_INIT, 1 );
		return;
	}

	void AOA_ATO_TGT::PredictTIG( int I, double incl, VECTOR3& RT, double& THETA )
	{
		// TIG from ET separation, adjusted for the MECO downrange dispersion when the reference angle is loaded
		double tigAdj = 0.0;
		double thetaLSS = pOMSBurnSoftware->GetTHETA_LSS();
		if ((THETA_LS_REF != 0.0) && (thetaLSS != 0.0)) tigAdj = (thetaLSS - THETA_LS_REF) / ORB_RATE;
		tigOPS3 = AOAOMS2Param( DTIG_AOA_OMS2, I, incl ) - tigAdj + ReadCOMPOOL_SD( SCP_T_ET_SEP );

		VECTOR3 R, V;
		double T;
		CurrentState( R, V, T );
		pStateVector->ONORBIT_PREDICT( R, V, T, tigOPS3, 4, 4, true, true, 1, DT_AOA_PRED, RGD, VGD );

		// H-theta-to-M50 with the AOA OMS-2 discrete: theta from the launch site to theta from TIG, target on the ellipsoid
		VECTOR3 IR = unit( RGD );
		VECTOR3 IDR = unit( crossp( RGD, crossp( VGD, RGD ) ) );
		VECTOR3 RLS = pStateVector->GetPositionAtT0();
		double thetaLS = atan2( dotp( -RLS, IDR ), dotp( RLS, IR ) );
		if (thetaLS < 0.0) thetaLS += PI2;
		THETA = fmod( AOAOMS2Param( THETA_AOA_OMS2, I, incl ) - thetaLS, PI2 );
		if (THETA <= 0.0) THETA += PI2;

		VECTOR3 IRT = (IR * cos( THETA )) + (IDR * sin( THETA ));
		RT = IRT * (H_EI - pGNCUtilities->H_ELLIPSOID( IRT ));
		return;
	}

	void AOA_ATO_TGT::AOA_ONORB_OMS2( void )
	{
		EnsureMasses();

		VECTOR3 IY = pAscentDAP->GetIY();
		double incl = acos( range( -1.0, -IY.z, 1.0 ) );
		double M = VehicleMass();
		double dmOMS1 = (MASS_ORB - DM_PREMECO()) - M;
		massOMSPreOMS2 = MASS_OMS_INIT - DM_PREMECO() - MASS_CG_CNTL - dmOMS1;

		int I;
		bool defaultPossible = false;
		if (ReadCOMPOOL_IS( SCP_AOA_TGT_SEL ) != 0)
		{
			I = ReadCOMPOOL_IS( SCP_I_TGT ) - 2;
			WriteCOMPOOL_IS( SCP_AOA_TGT_SEL, 0 );
		}
		else
		{
			if (oms1Index == 3) I = 1;
			else if (oms1Index == 4)
			{
				I = 2;
				defaultPossible = true;
			}
			else if (oms1Index == 5) I = 3;
			else I = 10;// the AOA was declared after the OMS-1 execute command
		}
		if ((I < 1) || (I > 10)) return;

		VECTOR3 RT;
		double THETA;
		PredictTIG( I, incl, RT, THETA );

		if (defaultPossible)
		{
			// projected OMS-2 delta V, two OMS engines
			double C1 = AOAOMS2Param( C1_AOA_OMS2, I, incl );
			double C2 = AOAOMS2Param( C2_AOA_OMS2, I, incl );
			peg4.SetPEG4Targets( C1, C2, RGD, VGD, tigOPS3, RT, 2.0 * FT_OMS, VEX_OMS, static_cast<float>(M), 0.0f, 0, NMAX_DIP );
			aoaI = I;
			aoaIncl = incl;
			aoaTheta = THETA;
			aoaM = M;
			pegBusy = true;
			return;
		}

		OutputOMS2( I, incl, THETA );
		return;
	}

	void AOA_ATO_TGT::AOA_OMS2_Finish( double VGOMAG )
	{
		if (GetMajorMode() != 105) return;

		double dvexcess = DV_OMS( aoaM, massOMSPreOMS2 ) - VGOMAG + DV_TOL;
		oapiWriteLogV( "(SSV_OV) [INFO] AOA OMS-2 excess delta V %.0f fps", dvexcess );

		int I = aoaI;
		double THETA = aoaTheta;
		if (dvexcess < 0.0)
		{
			// shallow EI default
			I = 6;
			VECTOR3 RT;
			PredictTIG( I, aoaIncl, RT, THETA );
		}
		OutputOMS2( I, aoaIncl, THETA );
		return;
	}

	void AOA_ATO_TGT::OutputOMS2( int I, double incl, double THETA )
	{
		double C1 = AOAOMS2Param( C1_AOA_OMS2, I, incl );
		double C2 = AOAOMS2Param( C2_AOA_OMS2, I, incl );

		// OMS propellant to deplete out of plane in the OMS-2 burn (the OPS 3 PRPLT item), sign for the side
		double wt = (massOMSPreOMS2 > 0.0) ? (SFUELD * G_2_FPS2 * massOMSPreOMS2) : 0.0;

		pOMSBurnSoftware->SetAbortTarget( H_EI, THETA, C1, C2, tigOPS3 - ReadCOMPOOL_SD( SCP_T_ET_SEP ), wt, true );
		WriteCOMPOOL_IS( SCP_I_TGT, I + 2 );
		WriteCOMPOOL_IS( SCP_S_AOA_OMS2, 1 );
		WriteCOMPOOL_IS( SCP_ABORT_INIT, 1 );
		return;
	}

	bool AOA_ATO_TGT::OnParseLine( const char* keyword, const char* value )
	{
		if (!_strnicmp( keyword, "STATE", 5 ))
		{
			int am = 0, mo = 0, mi = 0, sc = 0;
			sscanf_s( value, "%d %d %d %d %d", &am, &oms1Index, &mo, &mi, &sc );
			atoMECO = (am != 0);
			massOrbSet = (mo != 0);
			massOMSInitSet = (mi != 0);
			scheduled = (sc != 0);
			return true;
		}
		else if (!_strnicmp( keyword, "MASS", 4 ))
		{
			sscanf_s( value, "%lf %lf", &MASS_ORB, &MASS_OMS_INIT );
			return true;
		}
		return false;
	}

	void AOA_ATO_TGT::OnSaveState( FILEHANDLE scn ) const
	{
		char cbuf[128];
		sprintf_s( cbuf, 128, "%d %d %d %d %d", atoMECO, oms1Index, massOrbSet, massOMSInitSet, scheduled || pegBusy );
		oapiWriteScenario_string( scn, "STATE", cbuf );
		sprintf_s( cbuf, 128, "%.2f %.2f", MASS_ORB, MASS_OMS_INIT );
		oapiWriteScenario_string( scn, "MASS", cbuf );
		return;
	}
}
