/******* SSV File Modification Notice *******
Date         Developer
2020/03/29   GLS
2020/04/01   GLS
2020/06/14   GLS
2020/06/20   GLS
2020/08/24   GLS
2021/08/24   GLS
2021/10/23   GLS
2021/12/30   GLS
2022/06/13   GLS
2022/08/05   GLS
2022/09/29   GLS
2026/10/15   racerx2
********************************************/
#include "CrawlerPBI.h"
#include "Crawler.h"
#include <MathSSV.h>


constexpr double PUSH_LENGTH = 0.0015;


namespace vc
{
	CrawlerPBI::CrawlerPBI( Crawler* _v, const string& _ident ):CrawlerPanelLight( _v, _ident )
	{
		anim_pb = NULL;
		pPushDown = NULL;
		motionlength = PUSH_LENGTH;
	}

	CrawlerPBI::~CrawlerPBI()
	{
		if (pPushDown) delete pPushDown;
	}

	void CrawlerPBI::DefineVCAnimations( UINT vc_idx )
	{
		if (bHasDirection && !bHasAnimations)
		{
#if _DEBUG
			oapiWriteLogV( "CrawlerPBI[%s]:\tDefine VC Animations()", GetQualifiedIdentifier().c_str() );
#endif// _DEBUG

			anim_pb = V()->CreateAnimation( InitialAnimState() );

			pPushDown = new MGROUP_TRANSLATE( vc_idx, &grpIndex, 1, Normalize( GetDirection() ) * motionlength );
			V()->AddAnimationComponent( anim_pb, 0.0, 1.0, pPushDown );

			VerifyAnimations();
		}
	}

	bool CrawlerPBI::OnMouseEvent( int _event, float x, float y )
	{
		if (_event & PANEL_MOUSE_LBDOWN)
		{
			OnPress();
			return true;
		}
		else if (_event & PANEL_MOUSE_LBUP)
		{
			OnRelease();
			return true;
		}
		return false;
	}

	void CrawlerPBI::OnPress()
	{
		if (bHasAnimations) SetAnimation( anim_pb, 1.0 );
		output.SetLine();
		//PlayVesselWave( STS()->GetSoundID(), KEY_PRESS_SOUND );
		pressed = true;// not upstream: flight recorder (design ssv-flightrec)
	}

	void CrawlerPBI::OnRelease()
	{
		if (bHasAnimations) SetAnimation( anim_pb, 0.0 );
		output.ResetLine();
		pressed = false;// not upstream: as above
	}

	bool CrawlerPBI::GetPlaybackState( string& state ) const// not upstream: as declared
	{
		state = pressed ? "1" : "0";
		return true;
	}

	void CrawlerPBI::OnPlaybackEvent( double simt, double fEventT, const char* event_t, const char* event )// not upstream: as declared
	{
		const char* s = flightrec::SkipBlanks( event );
		if ((*s != '0') && (*s != '1')) return;
		if ((*s == '1') == pressed) return;
		if (*s == '1') OnPress();
		else OnRelease();
		return;
	}

	void CrawlerPBI::ConnectPushButton( DiscreteBundle* pBundle, unsigned short usLine )
	{
		output.Connect( pBundle, usLine );
		return;
	}

	void CrawlerPBI::SetMotionLength( double _motionlength )
	{
		motionlength = _motionlength;
		return;
	}

	CrawlerPBI2::CrawlerPBI2( Crawler* _v, const string& _ident ):CrawlerPanelLight2( _v, _ident )
	{
		anim_pb = NULL;
		pPushDown = NULL;
		motionlength = PUSH_LENGTH;
	}

	CrawlerPBI2::~CrawlerPBI2()
	{
		if (pPushDown) delete pPushDown;
	}

	void CrawlerPBI2::DefineVCAnimations( UINT vc_idx )
	{
		if (bHasDirection && !bHasAnimations)
		{
#if _DEBUG
			oapiWriteLogV( "PUSH BUTTON INDICATOR[%s]:\tDefine VC Animations()", GetQualifiedIdentifier().c_str() );
#endif// _DEBUG

			anim_pb = V()->CreateAnimation( InitialAnimState() );

			pPushDown = new MGROUP_TRANSLATE( vc_idx, &grpIndex, 1, Normalize( GetDirection() ) * motionlength );
			V()->AddAnimationComponent( anim_pb, 0.0, 1.0, pPushDown );

			VerifyAnimations();
		}
	}

	bool CrawlerPBI2::OnMouseEvent( int _event, float x, float y )
	{
		if (_event & PANEL_MOUSE_LBDOWN)
		{
			OnPress();
			return true;
		}
		else if (_event & PANEL_MOUSE_LBUP)
		{
			OnRelease();
			return true;
		}
		return false;
	}

	void CrawlerPBI2::OnPress()
	{
		if (bHasAnimations) SetAnimation( anim_pb, 1.0 );
		output.SetLine();
		//PlayVesselWave( STS()->GetSoundID(), KEY_PRESS_SOUND );
		pressed = true;// not upstream: flight recorder (design ssv-flightrec)
	}

	void CrawlerPBI2::OnRelease()
	{
		if (bHasAnimations) SetAnimation( anim_pb, 0.0 );
		output.ResetLine();
		pressed = false;// not upstream: as above
	}

	bool CrawlerPBI2::GetPlaybackState( string& state ) const// not upstream: as declared
	{
		state = pressed ? "1" : "0";
		return true;
	}

	void CrawlerPBI2::OnPlaybackEvent( double simt, double fEventT, const char* event_t, const char* event )// not upstream: as declared
	{
		const char* s = flightrec::SkipBlanks( event );
		if ((*s != '0') && (*s != '1')) return;
		if ((*s == '1') == pressed) return;
		if (*s == '1') OnPress();
		else OnRelease();
		return;
	}

	void CrawlerPBI2::ConnectPushButton( DiscreteBundle* pBundle, unsigned short usLine )
	{
		output.Connect( pBundle, usLine );
		return;
	}

	void CrawlerPBI2::SetMotionLength( double _motionlength )
	{
		motionlength = _motionlength;
		return;
	}
}
