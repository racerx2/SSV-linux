/******* SSV File Modification Notice *******
Date         Developer
2020/03/29   GLS
2020/04/01   GLS
2020/06/14   GLS
2020/06/20   GLS
2020/08/24   GLS
2021/08/24   GLS
2022/06/13   GLS
2022/08/05   GLS
2022/09/29   GLS
2026/10/15   racerx2
********************************************/
#include "CrawlerPushButton.h"
#include "Crawler.h"
#include <MathSSV.h>


const double PUSH_LENGTH = 0.0015;


namespace vc
{
	CrawlerPushButton::CrawlerPushButton( Crawler* _v, const string& _ident ):BasicVCComponent( _v, _ident ), output()
	{
		anim_pb = NULL;
		pPushDown = NULL;
		uiGroup = 0xFFFF;
		motionlength = PUSH_LENGTH;
	}


	CrawlerPushButton::~CrawlerPushButton()
	{
		if (pPushDown) delete pPushDown;
	}

	void CrawlerPushButton::DefineGroup(UINT _grpIndex) {
		uiGroup = _grpIndex;
	}

	void CrawlerPushButton::DefineVCAnimations(UINT vc_idx) {
		if(bHasDirection && !bHasAnimations) {
#if _DEBUG
			oapiWriteLogV( "CRAWLER PUSH BUTTON[%s]:\tDefine VC Animations()",
				GetQualifiedIdentifier().c_str() );
#endif// _DEBUG

			anim_pb = V()->CreateAnimation(InitialAnimState());

			pPushDown = new MGROUP_TRANSLATE(vc_idx, &uiGroup, 1,
				Normalize( GetDirection() ) * motionlength );
			V()->AddAnimationComponent(anim_pb, 0.0, 1.0, pPushDown);

			VerifyAnimations();
		}
	}

	bool CrawlerPushButton::OnMouseEvent(int _event, float x, float y) {
		if(_event & PANEL_MOUSE_LBDOWN) {
			OnPress();
			return true;
		}
		else if(_event & PANEL_MOUSE_LBUP) {
			OnDepress();
			return true;
		}
		return false;

	}

	void CrawlerPushButton::Realize() {
	}

	void CrawlerPushButton::OnPress() {
		if(bHasAnimations)
		{
			SetAnimation(anim_pb, 1.0);
		}
		output.SetLine();
		//PlayVesselWave( STS()->GetSoundID(), KEY_PRESS_SOUND );
		pressed = true;// not upstream: flight recorder (design ssv-flightrec)
	}

	void CrawlerPushButton::OnDepress() {
		if(bHasAnimations)
		{
			SetAnimation(anim_pb, 0.0);
		}
		output.ResetLine();
		pressed = false;// not upstream: as above
	}

	bool CrawlerPushButton::GetPlaybackState( string& state ) const// not upstream: as declared
	{
		state = pressed ? "1" : "0";
		return true;
	}

	void CrawlerPushButton::OnPlaybackEvent( double simt, double fEventT, const char* event_t, const char* event )// not upstream: as declared
	{
		const char* s = flightrec::SkipBlanks( event );
		if ((*s != '0') && (*s != '1')) return;
		if ((*s == '1') == pressed) return;
		if (*s == '1') OnPress();
		else OnDepress();
		return;
	}

	void CrawlerPushButton::SetMotionLength( double _motionlength )
	{
		motionlength = _motionlength;
		return;
	}
}
