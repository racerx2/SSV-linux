/******* SSV File Modification Notice *******
Date         Developer
2020/03/29   GLS
2020/05/07   GLS
2020/05/08   GLS
2020/05/10   GLS
2020/06/20   GLS
2020/08/24   GLS
2021/08/24   GLS
2021/10/23   GLS
2021/12/30   GLS
2022/06/13   GLS
2022/08/05   GLS
2022/09/29   GLS
2022/10/04   GLS
2024/12/30   GLS
2026/10/15   racerx2
********************************************/
#include "../Atlantis.h"
#include "PushButton.h"
#include "../../SSVSound.h"
#include <MathSSV.h>

namespace vc
{
	PushButton::PushButton( Atlantis* _sts, const std::string& _name )
		: AtlantisVCComponent( _sts, _name ), output()
	{
		anim_pb = NULL;
		pPushDown = NULL;
		uiGroup = 0xFFFF;
		motionlength = PUSH_LENGTH;
	}


	PushButton::~PushButton()
	{
		if (pPushDown) delete pPushDown;
	}

	void PushButton::DefineGroup(UINT _grpIndex) {
		uiGroup = _grpIndex;
	}

	void PushButton::DefineVCAnimations(UINT vc_idx) {
		if(bHasDirection && !bHasAnimations) {
#if _DEBUG
			oapiWriteLogV( "PUSH BUTTON[%s]:\tDefine VC Animations()", GetQualifiedIdentifier().c_str() );
#endif// _DEBUG

			anim_pb = STS()->CreateAnimation(InitialAnimState());

			pPushDown = new MGROUP_TRANSLATE(vc_idx, &uiGroup, 1,
				Normalize( GetDirection() ) * motionlength );
			STS()->AddAnimationComponent(anim_pb, 0.0, 1.0, pPushDown);

			VerifyAnimations();
		}
	}

	bool PushButton::OnMouseEvent(int _event, float x, float y) {
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

	void PushButton::OnPress() {
		if(bHasAnimations)
		{
			SetAnimation(anim_pb, 1.0);
		}
		output.SetLine();
		SoundPlay( STS()->GetSound(), KEY_PRESS_SOUND );
		bPressed = true;// not upstream: flight recorder (design ssv-flightrec)
	}

	void PushButton::OnDepress() {
		if(bHasAnimations)
		{
			SetAnimation(anim_pb, 0.0);
		}
		output.ResetLine();
		bPressed = false;// not upstream: as above
	}

	bool PushButton::GetPlaybackState( string& state ) const// not upstream: flight recorder, 1 pressed or 0 (design ssv-flightrec)
	{
		state = bPressed ? "1" : "0";
		return true;
	}

	void PushButton::OnPlaybackEvent( double simt, double fEventT, const char* event_t, const char* event )// not upstream: as above
	{
		const char* s = flightrec::SkipBlanks( event );
		if ((*s != '0') && (*s != '1')) return;
		const bool press = (*s == '1');
		if (press) bReleasePending = false;
		if (press == bPressed) return;
		if (press)
		{
			OnPress();
			fPressT = simt;
		}
		else if (simt <= fPressT) bReleasePending = true;
		else OnDepress();
		return;
	}

	void PushButton::OnPreStep( double simt, double simdt, double mjd )// not upstream: as above, the pending release
	{
		AtlantisVCComponent::OnPreStep( simt, simdt, mjd );
		if (bReleasePending && (simt > fPressT))
		{
			bReleasePending = false;
			if (bPressed) OnDepress();
		}
		return;
	}

	void PushButton::SetMotionLength( double _motionlength )
	{
		motionlength = _motionlength;
		return;
	}

	void PushButton::Connect( DiscreteBundle* pBundle, unsigned short usLine )
	{
		output.Connect( pBundle, usLine );
		return;
	}
}

