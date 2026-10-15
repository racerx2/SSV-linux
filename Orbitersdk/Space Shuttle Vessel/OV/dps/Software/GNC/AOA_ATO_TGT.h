/****************************************************************************
  This file is part of Space Shuttle Vessel

  AOA/ATO Targeting (FSSR 4.5) definition


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
#ifndef _dps_AOA_ATO_TGT_H_
#define _dps_AOA_ATO_TGT_H_


#include "../SimpleGPCSoftware.h"
#include <PEG4Targeting.h>


namespace dps
{
	class AscentDAP;
	class OMSBurnSoftware;
	class StateVectorSoftware;
	class GNCUtilities;

	// AOA/ATO targeting (STS 83-0002 4.5): ATO MECO targets and the pre-MECO OMS burn scale factor in MM 103, AOA and ATO OMS-1 targets in MM 104, OMS-2 targets in MM 105.
	// Scheduled by AbortModing.
	class AOA_ATO_TGT : public SimpleGPCSoftware
	{
		private:
			AscentDAP* pAscentDAP;
			OMSBurnSoftware* pOMSBurnSoftware;
			StateVectorSoftware* pStateVector;
			GNCUtilities* pGNCUtilities;

			PEG4Targeting peg4;

			// I-loads (FSSR table 4.5.6-3), ft, fps, rad, s, slug
			double V_ATO_SW;
			bool bATOMECOILoad;
			double RD_ATO;
			double VD_ATO;
			double GAMD_ATO;
			int S_IY;
			VECTOR3 IY_MIN_EF;
			double V_MSSN_CNTN;
			double V_LIN;
			double V_ZERO;
			double EPS_K2;
			double MASS_ORB;
			double MASS_OMS_INIT;
			double MASS_CG_CNTL;
			double DV_RQD;
			double DV_TOL;
			int SFUELD;
			double THETA_LS_REF;
			double HTGT_ATO_OMS1, THETA_ATO_OMS1, C1_ATO_OMS1, C2_ATO_OMS1, DTIG_ATO_OMS1;
			double HTGT_ATO_OMS2[2], THETA_ATO_OMS2[2], C1_ATO_OMS2[2], C2_ATO_OMS2[2], DTIG_ATO_OMS2[2];
			double HTGT_AOA_OMS1[3], THETA_AOA_OMS1[3], C1_AOA_OMS1[3], C2_AOA_OMS1[3], DTIG_AOA_OMS1[3];
			double THETA_AOA_OMS2[30], C1_AOA_OMS2[30], C2_AOA_OMS2[30], DTIG_AOA_OMS2[30];// [I][constant, linear, quadratic in inclination]

			// state
			bool scheduled;
			bool atoMECO;// ATO MECO targets were selected
			int oms1Index;// I_TGT of the OMS-1 target set, 0 if none
			bool massOrbSet;
			bool massOMSInitSet;

			// AOA OMS-2 selection in progress (PEG-4 for the projected OMS-2 delta V)
			bool pegBusy;
			int aoaI;
			double aoaIncl;
			double aoaTheta;
			double aoaM;
			double tigOPS3;
			double massOMSPreOMS2;
			VECTOR3 RGD, VGD;

			void Sequence( void );
			void ATO_MECO_TGT_SEL( void );
			void ATO_OMS_PREMECO_BRN_DET( void );
			void AOA_ONORB_OMS1( void );
			void ATO_ONORB_OMS1( void );
			void AOA_ONORB_OMS2( void );
			void AOA_OMS2_Finish( double VGOMAG );
			void ATO_ONORB_OMS2( void );
			void CurrentState( VECTOR3& R, VECTOR3& V, double& T );
			void EnsureMasses( void );
			double VehicleMass( void ) const;
			double DM_PREMECO( void ) const;
			double DV_OMS( double M, double massOMS ) const;
			double AOAOMS2Param( const double* table, int I, double incl ) const;
			void PredictTIG( int I, double incl, VECTOR3& RT, double& THETA );
			void OutputOMS2( int I, double incl, double THETA );

		public:
			explicit AOA_ATO_TGT( SimpleGPCSystem* _gpc );
			virtual ~AOA_ATO_TGT( void );

			void Realize( void ) override;
			void ReadILOADs( const std::map<std::string,std::string>& ILOADs ) override;
			void OnPreStep( double simt, double simdt, double mjd ) override;
			bool OnMajorModeChange( unsigned int newMajorMode ) override;
			bool OnParseLine( const char* keyword, const char* value ) override;
			void OnSaveState( FILEHANDLE scn ) const override;

			// MSC scheduling (events A5A, A9.1, A13, A16, A17, A19)
			void Schedule( void ) { scheduled = true; };
			int GetOMS1Index( void ) const { return oms1Index; };
	};
}


#endif// _dps_AOA_ATO_TGT_H_
