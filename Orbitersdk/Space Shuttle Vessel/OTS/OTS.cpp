/******* SSV File Modification Notice *******
Date         Developer
2020/03/20   GLS
2020/03/29   GLS
2020/04/01   GLS
2020/05/01   GLS
2020/06/01   GLS
2020/06/12   GLS
2020/06/20   GLS
2020/06/21   GLS
2020/06/23   GLS
2021/08/23   GLS
2021/08/24   GLS
2022/01/10   GLS
2022/01/15   GLS
2022/08/05   GLS
2022/09/29   GLS
2026/10/15   racerx2
********************************************/
#define ORBITER_MODULE


#include "OTS.h"
#include <SurfaceRoving.h>
#include <cmath>// not upstream: flight recorder (design ssv-flightrec)


const static char* MESHNAME_OTS = "SSV\\OTS\\OTS";



DLLCLBK VESSEL *ovcInit( OBJHANDLE vessel, int flightmodel )
{
	try
	{
		return new OTS( vessel );
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in ovcInit: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in ovcInit" );
		abort();
	}
}

DLLCLBK void ovcExit( VESSEL *vessel )
{
	try
	{
		if (vessel) delete static_cast<OTS*>(vessel);
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in ovcExit: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in ovcExit" );
		abort();
	}
}


OTS::OTS( OBJHANDLE hVessel ):VESSEL4( hVessel )
{
	surfaceroving = new SurfaceRoving( this, 9.3175, 5.0, -0.003351 );

	fwdsteer = 0.0;
	aftsteer = 0.0;
	drive = 0.0;
	return;
}

OTS::~OTS( void )
{
	return;
}

void OTS::clbkSetClassCaps( FILEHANDLE cfg )
{
	try
	{
		hMesh = oapiLoadMeshGlobal( MESHNAME_OTS );
		mesh_idx = AddMesh( hMesh );
		SetMeshVisibilityMode( mesh_idx, MESHVIS_ALWAYS );
		SetSize( 30.0 );
		SetEmptyMass( 2721000/*50000.0*/ );
		//SetCrossSections( _V( 19.49, 18.88, 15.71 ) );
		//SetPMI( _V( 4.32, 4.35, 2.69 ) );
		SetCameraOffset( _V( -0.45, 1.7, 14.3 ) );

		DWORD ntdvtx = 4;
		static TOUCHDOWNVTX tdvtx[4] ={
			{_V( -1.558297, 0.003351, 9.3175 ), 1e8, 2e7, 0.5, 0.005},
			{_V( 1.558297, 0.003351, 9.3175 ), 1e8, 2e7, 0.5, 0.005},
			{_V( 1.558297, 0.003351, -9.3175 ), 1e8, 2e7, 0.5, 0.005},
			{_V( -1.558297, 0.003351, -9.3175 ), 1e8, 2e7, 0.5},
		};
		SetTouchdownPoints( tdvtx, ntdvtx );

		ahToOV = CreateAttachment( false, _V( 0, 0, 3.3602 ), _V( 0, 0, 1 ), _V( 0, 1, 0 ), "SSV_OV" );

		DefineAnimations();

		/////////////////////////////////////////////////////////////////////////////////////////
		VECTOR3 ofs = _V( 0.0, 7.0, 2.0 );
		SetMeshVisibilityMode( AddMesh( oapiLoadMeshGlobal( "SSV\\OV\\OV" ), &ofs ), MESHVIS_ALWAYS );
		/////////////////////////////////////////////////////////////////////////////////////////
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkSetClassCaps: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkSetClassCaps" );
		abort();
	}
}

void OTS::clbkPostCreation( void )
{
	try
	{
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkPostCreation: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkPostCreation" );
		abort();
	}
	return;
}

void OTS::clbkPreStep( double simt, double simdt, double mjd )
{
	try
	{
		flightrecorder.Step();// not upstream: flight recorder (design ssv-flightrec)
		if (flightrecorder.JustStarted()) RecordDrive();// not upstream: as above

		if (aftsteer != 0.0) surfaceroving->Move_Crab( simdt, -aftsteer );
		else surfaceroving->Move( simdt, drive, fwdsteer, 0.0/*aftsteer*/ );

		//sprintf_s( oapiDebugString(), 256, "%.2f %.2f %.2f", drive, fwdsteer, aftsteer );
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkPreStep: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkPreStep" );
		abort();
	}
}

void OTS::DefineAnimations( void )
{
	try
	{
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::DefineAnimations: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::DefineAnimations" );
		abort();
	}
}

int OTS::clbkConsumeBufferedKey( DWORD key, bool down, char* kstate )
{
	try
	{
		if (!down)
		{
			// man gimbal
			if (key == OAPI_KEY_NUMPAD7)
			{
				Drive( fwdsteer, -2.0 );// not upstream: flight recorder (design ssv-flightrec)
				return 1;
			}
			else if (key == OAPI_KEY_NUMPAD9)
			{
				Drive( fwdsteer, 2.0 );// not upstream: as above
				return 1;
			}
			else if (key == OAPI_KEY_NUMPAD1)
			{
				Drive( aftsteer, 2.0 );// not upstream: as above
				return 1;
			}
			else if (key == OAPI_KEY_NUMPAD3)
			{
				Drive( aftsteer, -2.0 );// not upstream: as above
				return 1;
			}
			else if (key == OAPI_KEY_NUMPAD8)
			{
				Drive( drive, 1.0 );// not upstream: as above
				return 1;
			}
			else if (key == OAPI_KEY_NUMPAD2)
			{
				Drive( drive, -1.0 );// not upstream: as above
				return 1;
			}
			return 0;
		}
		return 0;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkConsumeBufferedKey: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkConsumeBufferedKey" );
		abort();
	}
}

bool OTS::clbkPlaybackEvent( double simt, double event_t, const char* event_type, const char* event )// not upstream: as declared
{
	try
	{
		if (!event) event = "";
		if (flightrecorder.OnPlaybackEvent( event_type, event )) return true;

		if (!_stricmp( event_type, flightrec::CTRL ))
		{
			double c[3];
			if ((sscanf_s( event, "%lf %lf %lf", &c[0], &c[1], &c[2] ) == 3) && std::isfinite( c[0] ) && std::isfinite( c[1] ) && std::isfinite( c[2] ))
			{
				fwdsteer = c[0];
				aftsteer = c[1];
				drive = c[2];
			}
			else flightrec::LogUnknown( "SSV_OTS", event_type, "(bad line)" );
			return true;
		}
		return false;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkPlaybackEvent: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkPlaybackEvent" );
		abort();
	}
}

void OTS::Drive( double& value, double d )// not upstream: as declared
{
	if (flightrecorder.Locked()) return;
	value += d;
	RecordDrive();
	return;
}

void OTS::RecordDrive( void ) const// not upstream: as declared
{
	if (!flightrecorder.Recording()) return;
	char cbuf[128];
	sprintf_s( cbuf, sizeof(cbuf), "%.6f %.6f %.6f", fwdsteer, aftsteer, drive );
	flightrecorder.Record( flightrec::CTRL, cbuf );
	return;
}

void OTS::clbkVisualCreated( VISHANDLE vis, int refcount )
{
	try
	{
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkVisualCreated: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkVisualCreated" );
		abort();
	}
}

void OTS::clbkSaveState( FILEHANDLE scn )
{
	try
	{
		VESSEL4::clbkSaveState( scn );

		//if (strMission.length() > 0) oapiWriteScenario_string( scn, "MISSION", (char*)strMission.c_str() );
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkSaveState: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkSaveState" );
		abort();
	}
}

void OTS::clbkLoadStateEx( FILEHANDLE scn, void *status )
{
	try
	{
		char* line;

		while (oapiReadScenario_nextline( scn, line ))
		{
			/*if (!_strnicmp( line, "MISSION", 7 ))
			{
				strMission = line + 8;
				LoadMissionFile();
			}
			else*/ ParseScenarioLineEx( line, status );
		}
		return;
	}
	catch (std::exception &e)
	{
		oapiWriteLogV( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkLoadStateEx: %s", e.what() );
		abort();
	}
	catch (...)
	{
		oapiWriteLog( "(SSV_OTS) [FATAL ERROR] Exception in OTS::clbkLoadStateEx" );
		abort();
	}
}
