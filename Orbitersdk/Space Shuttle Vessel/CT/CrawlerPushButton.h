/****************************************************************************
  This file is part of Space Shuttle Vessel

  CrawlerPushButton definition


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
2020/04/01   GLS
2020/05/10   GLS
2020/06/14   GLS
2020/06/20   GLS
2021/01/20   GLS
2021/08/24   GLS
2021/12/25   GLS
2022/09/29   GLS
2025/01/04   GLS
2026/10/15   racerx2
********************************************/

#ifndef CRAWLERPUSHBUTTON_H_
#define CRAWLERPUSHBUTTON_H_


#include <BasicVCComponent.h>
#include <DiscOutPort.h>


class Crawler;


namespace vc
{
	using discsignals::DiscOutPort;

	// not upstream: flight recorder state of a momentary button, 1 pressed or 0 (design ssv-flightrec)
	class MomentaryPlayback
	{
		public:
			bool pressed = false;

			bool GetState( string& state ) const
			{
				state = pressed ? "1" : "0";
				return true;
			}

			// 1 press, 0 release, -1 nothing now; a release in the step of its press waits for ReleaseDue
			int OnEvent( double simt, const char* event )
			{
				const char* s = flightrec::SkipBlanks( event );
				if ((*s != '0') && (*s != '1')) return -1;
				if ((*s == '1') == pressed) return -1;
				releaseSteps = 0;
				if (*s == '1')
				{
					pressT = simt;
					return 1;
				}
				if (simt != pressT) return 0;
				releaseSteps = 2;// the crawler reads the lines after the panels' OnPreStep
				return -1;
			}

			// once per OnPreStep: true when the held release is due
			bool ReleaseDue( void )
			{
				return (releaseSteps > 0) && (--releaseSteps == 0);
			}

		private:
			double pressT = -1.0;
			int releaseSteps = 0;
	};

	class CrawlerPushButton : public BasicVCComponent<Crawler>
	{
		private:
			UINT anim_pb;
			UINT uiGroup;
			double motionlength;
			MGROUP_TRANSLATE* pPushDown;
		public:
			CrawlerPushButton( Crawler* _v, const string& _ident );
			virtual ~CrawlerPushButton();

			void DefineGroup(UINT _grpIndex);
			void DefineVCAnimations(UINT vc_idx) override;
			bool OnMouseEvent(int _event, float x, float y) override;
			virtual void Realize() override;
			virtual void OnPress();
			virtual void OnDepress();
			bool GetPlaybackState( string& state ) const override;// not upstream: flight recorder, 1 pressed or 0 (design ssv-flightrec)
			void OnPlaybackEvent( double simt, double fEventT, const char* event_t, const char* event ) override;// not upstream: as above
			void OnPreStep( double simt, double simdt, double mjd ) override;// not upstream: as above

			void SetMotionLength( double _motionlength );

			DiscOutPort output;
		private:// not upstream: flight recorder (design ssv-flightrec)
			MomentaryPlayback playback;// not upstream: as above
	};
}

#endif// CRAWLERPUSHBUTTON_H_
