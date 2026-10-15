/****************************************************************************
  This file is part of Space Shuttle Vessel

  Abort moding (MSC abort declaration and AOA/ATO targeting scheduling) implementation


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
#include "AbortModing.h"
#include "AscentDAP.h"
#include "OMSBurnSoftware.h"
#include "AOA_ATO_TGT.h"
#include <cassert>


namespace dps
{
	AbortModing::AbortModing( SimpleGPCSystem* _gpc ):SimpleGPCSoftware( _gpc, "AbortModing" ),
		pAscentDAP( NULL ), pOMSBurnSoftware( NULL ), pAOA_ATO_TGT( NULL ), tgtItemUsed( false )
	{
		Reset();
		return;
	}

	void AbortModing::Reset( void )
	{
		prevReq[0] = prevReq[1] = prevReq[2] = false;
		logged[0] = logged[1] = false;
		tgtItemUsed = false;
		return;
	}

	AbortModing::~AbortModing( void )
	{
		return;
	}

	void AbortModing::Realize( void )
	{
		pAscentDAP = dynamic_cast<AscentDAP*>(FindSoftware( "AscentDAP" ));
		assert( (pAscentDAP != NULL) && "AbortModing::Realize.pAscentDAP" );
		pOMSBurnSoftware = dynamic_cast<OMSBurnSoftware*>(FindSoftware( "OMSBurnSoftware" ));
		assert( (pOMSBurnSoftware != NULL) && "AbortModing::Realize.pOMSBurnSoftware" );
		pAOA_ATO_TGT = dynamic_cast<AOA_ATO_TGT*>(FindSoftware( "AOA_ATO_TGT" ));
		assert( (pAOA_ATO_TGT != NULL) && "AbortModing::Realize.pAOA_ATO_TGT" );
		return;
	}

	bool AbortModing::OnMajorModeChange( unsigned int newMajorMode )
	{
		if (GetMajorMode() != 0)
		{
			bool abort = (ReadCOMPOOL_IS( SCP_S_ATO ) != 0) || (ReadCOMPOOL_IS( SCP_S_AOA ) != 0);
			if (newMajorMode == 101)
			{
				WriteCOMPOOL_IS( SCP_S_ATO, 0 );
				WriteCOMPOOL_IS( SCP_S_AOA, 0 );
				WriteCOMPOOL_IS( SCP_I_TGT, 0 );
				WriteCOMPOOL_IS( SCP_AOA_TGT_SEL, 0 );
				WriteCOMPOOL_IS( SCP_ATO_TGT_SEL, 0 );
				WriteCOMPOOL_IS( SCP_OVRD_ABORT_SEL, 0 );
				WriteCOMPOOL_IS( SCP_OVRD_ABORT_INIT, 0 );
				WriteCOMPOOL_IS( SCP_ABORT_INIT, 0 );
				WriteCOMPOOL_IS( SCP_S_ABORT_CONTROL, 0 );
				WriteCOMPOOL_SS( SCP_K2, 0.0f );
				WriteCOMPOOL_IS( SCP_S_ATO_IY, 0 );
				WriteCOMPOOL_IS( SCP_S_AOA_OMS2, 0 );
				WriteCOMPOOL_SS( SCP_V_ME_OUT, 0.0f );
				Reset();
			}
			else if (newMajorMode == 103)
			{
				// event A5A: an ATO selected in first stage
				if (ReadCOMPOOL_IS( SCP_S_ATO ) != 0) pAOA_ATO_TGT->Schedule();
			}
			else if (newMajorMode == 104)
			{
				// the target index starts at zero in MM 104; event A13
				WriteCOMPOOL_IS( SCP_I_TGT, 0 );
				if (abort) pAOA_ATO_TGT->Schedule();
			}
			else if (newMajorMode == 105)
			{
				// event A17
				if (abort) pAOA_ATO_TGT->Schedule();
			}
		}

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

	void AbortModing::OnPreStep( double simt, double simdt, double mjd )
	{
		if ((GetMajorMode() / 100) != 1) return;

		// ABORT pushbutton with the ABORT MODE rotary
		bool req[3] = {
			(ReadCOMPOOL_IS( SCP_ATO_ABORT_REQUEST ) != 0),
			(ReadCOMPOOL_IS( SCP_TAL_ABORT_REQUEST ) != 0),
			(ReadCOMPOOL_IS( SCP_RTLS_ABORT_REQUEST ) != 0)};
		if (req[0] && !prevReq[0] && !tgtItemUsed) DeclareATO();
		if (req[1] && !prevReq[1]) NotAvailable( 0 );
		if (req[2] && !prevReq[2]) NotAvailable( 1 );
		for (int k = 0; k < 3; k++) prevReq[k] = req[k];

		// OVERRIDE display: item 3 (1 requested, 2 initiated) initiates the abort selected by item 1 or 2
		if (ReadCOMPOOL_IS( SCP_OVRD_ABORT_INIT ) == 1)
		{
			WriteCOMPOOL_IS( SCP_OVRD_ABORT_INIT, 2 );
			if ((ReadCOMPOOL_IS( SCP_OVRD_ABORT_SEL ) == 2) && !tgtItemUsed) DeclareATO();
		}

		// maneuver display ITEM 35: AOA or ATO with a crew target set (events A16A, A19A), up or down mode, before the OMS-x execute command
		bool tgtSel = (ReadCOMPOOL_IS( SCP_AOA_TGT_SEL ) != 0) || (ReadCOMPOOL_IS( SCP_ATO_TGT_SEL ) != 0);
		unsigned int mm = GetMajorMode();
		if (tgtSel && (((mm != 104) && (mm != 105)) || pOMSBurnSoftware->BurnEnabled()))
		{
			WriteCOMPOOL_IS( SCP_AOA_TGT_SEL, 0 );
			WriteCOMPOOL_IS( SCP_ATO_TGT_SEL, 0 );
			oapiWriteLogV( "(SSV_OV) [INFO] ITEM 35 target %hu refused, MM %u", ReadCOMPOOL_IS( SCP_I_TGT ), mm );
		}
		else if (ReadCOMPOOL_IS( SCP_AOA_TGT_SEL ) != 0)
		{
			tgtItemUsed = true;
			if (ReadCOMPOOL_IS( SCP_S_AOA ) == 0)
			{
				WriteCOMPOOL_IS( SCP_S_AOA, 1 );
				WriteCOMPOOL_IS( SCP_S_ATO, 0 );
				oapiWriteLogV( "(SSV_OV) [INFO] AOA declared, target %hu, MM %u", ReadCOMPOOL_IS( SCP_I_TGT ), GetMajorMode() );
			}
			pAOA_ATO_TGT->Schedule();
		}
		else if (ReadCOMPOOL_IS( SCP_ATO_TGT_SEL ) != 0)
		{
			tgtItemUsed = true;
			if ((ReadCOMPOOL_IS( SCP_S_ATO ) == 0) || (ReadCOMPOOL_IS( SCP_S_AOA ) != 0))
			{
				WriteCOMPOOL_IS( SCP_S_ATO, 1 );
				WriteCOMPOOL_IS( SCP_S_AOA, 0 );
				WriteCOMPOOL_IS( SCP_S_AOA_OMS2, 0 );
				oapiWriteLogV( "(SSV_OV) [INFO] ATO declared, target %hu, MM %u", ReadCOMPOOL_IS( SCP_I_TGT ), GetMajorMode() );
			}
			pAOA_ATO_TGT->Schedule();
		}
		return;
	}

	void AbortModing::DeclareATO( void )
	{
		if ((ReadCOMPOOL_IS( SCP_S_ATO ) != 0) || (ReadCOMPOOL_IS( SCP_S_AOA ) != 0)) return;

		// an ATO is legal up to the OMS-2 execute command
		unsigned int mm = GetMajorMode();
		if ((mm < 102) || (mm > 105)) return;
		if ((mm == 105) && pOMSBurnSoftware->BurnEnabled()) return;

		WriteCOMPOOL_IS( SCP_S_ATO, 1 );
		oapiWriteLogV( "(SSV_OV) [INFO] ATO declared, MM %u", mm );

		// events A9.1, A16 and A19; in MM 102 at the MM 103 transition, after the OMS-1 execute command at the MM 105 transition
		if (mm == 103)
		{
			if (!pAscentDAP->GetFineCount()) pAOA_ATO_TGT->Schedule();
		}
		else if (mm == 104)
		{
			if (!pOMSBurnSoftware->BurnEnabled()) pAOA_ATO_TGT->Schedule();
		}
		else if (mm == 105) pAOA_ATO_TGT->Schedule();
		return;
	}

	void AbortModing::NotAvailable( int k )
	{
		if (logged[k]) return;
		logged[k] = true;
		oapiWriteLogV( "(SSV_OV) [INFO] %s abort request ignored: TAL and RTLS are not implemented yet", (k == 0) ? "TAL" : "RTLS" );
		return;
	}

	unsigned short AbortModing::GetAbortMode( void ) const
	{
		if (ReadCOMPOOL_IS( SCP_RTLS_ABORT_DECLARED ) != 0) return 4;
		if (ReadCOMPOOL_IS( SCP_TAL_ABORT_DECLARED ) != 0) return 3;
		if (ReadCOMPOOL_IS( SCP_S_AOA ) != 0) return 2;
		if (ReadCOMPOOL_IS( SCP_S_ATO ) != 0) return 1;
		return 0;
	}

	bool AbortModing::OnParseLine( const char* keyword, const char* value )
	{
		if (!_strnicmp( keyword, "FLAGS", 5 ))
		{
			unsigned short f[12] = {0};
			sscanf_s( value, "%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6], &f[7], &f[8], &f[9], &f[10], &f[11] );
			WriteCOMPOOL_IS( SCP_S_ATO, f[0] );
			WriteCOMPOOL_IS( SCP_S_AOA, f[1] );
			WriteCOMPOOL_IS( SCP_I_TGT, f[2] );
			WriteCOMPOOL_IS( SCP_OVRD_ABORT_SEL, f[3] );
			WriteCOMPOOL_IS( SCP_OVRD_ABORT_INIT, f[4] );
			tgtItemUsed = (f[5] != 0);
			WriteCOMPOOL_IS( SCP_S_ABORT_CONTROL, f[6] );
			WriteCOMPOOL_IS( SCP_S_ATO_IY, f[7] );
			WriteCOMPOOL_IS( SCP_S_AOA_OMS2, f[8] );
			WriteCOMPOOL_IS( SCP_ABORT_INIT, f[9] );
			WriteCOMPOOL_IS( SCP_AOA_TGT_SEL, f[10] );
			WriteCOMPOOL_IS( SCP_ATO_TGT_SEL, f[11] );
			return true;
		}
		else if (!_strnicmp( keyword, "K2", 2 ))
		{
			float k2 = 0.0f, vme = 0.0f;
			sscanf_s( value, "%f %f", &k2, &vme );
			WriteCOMPOOL_SS( SCP_K2, k2 );
			WriteCOMPOOL_SS( SCP_V_ME_OUT, vme );
			return true;
		}
		return false;
	}

	void AbortModing::OnSaveState( FILEHANDLE scn ) const
	{
		char cbuf[128];
		sprintf_s( cbuf, 128, "%hu %hu %hu %hu %hu %d %hu %hu %hu %hu %hu %hu", ReadCOMPOOL_IS( SCP_S_ATO ), ReadCOMPOOL_IS( SCP_S_AOA ), ReadCOMPOOL_IS( SCP_I_TGT ),
			ReadCOMPOOL_IS( SCP_OVRD_ABORT_SEL ), ReadCOMPOOL_IS( SCP_OVRD_ABORT_INIT ), tgtItemUsed, ReadCOMPOOL_IS( SCP_S_ABORT_CONTROL ), ReadCOMPOOL_IS( SCP_S_ATO_IY ),
			ReadCOMPOOL_IS( SCP_S_AOA_OMS2 ), ReadCOMPOOL_IS( SCP_ABORT_INIT ), ReadCOMPOOL_IS( SCP_AOA_TGT_SEL ), ReadCOMPOOL_IS( SCP_ATO_TGT_SEL ) );
		oapiWriteScenario_string( scn, "FLAGS", cbuf );
		sprintf_s( cbuf, 128, "%.4f %.1f", ReadCOMPOOL_SS( SCP_K2 ), ReadCOMPOOL_SS( SCP_V_ME_OUT ) );
		oapiWriteScenario_string( scn, "K2", cbuf );
		return;
	}
}
