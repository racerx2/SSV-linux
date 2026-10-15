/****************************************************************************
  This file is part of Space Shuttle Vessel

  OMS/RCS aft propellant feed valves interface


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
#ifndef __OMSRCS_INTERFACE_H_
#define __OMSRCS_INTERFACE_H_


namespace oms
{
	// valve groups of one pod: RCS tank isolation 1/2 and 3/4/5, RCS aft crossfeed 1/2 and 3/4/5, OMS crossfeed A and B
	enum OMSRCS_VLV { VLV_TK12, VLV_TK345, VLV_XFD12, VLV_XFD345, VLV_OMSXFDA, VLV_OMSXFDB, VLV_COUNT };

	// valve bundles of the left and right pods, 16 lines: open (command) or open indication at channel v, close (command) or closed indication at v + VLV_COUNT
	constexpr const char* OMSRCS_VLV_CMD_BUNDLE[2] = {"OMSRCS_VLV_L_CMD", "OMSRCS_VLV_R_CMD"};
	constexpr const char* OMSRCS_VLV_IND_BUNDLE[2] = {"OMSRCS_VLV_L_IND", "OMSRCS_VLV_R_IND"};
	constexpr unsigned short OMSRCS_VLV_BUNDLE_LINES = 16;

	constexpr unsigned short OMSRCS_VLV_CH_OPEN( unsigned int v ) { return static_cast<unsigned short>(v); }
	constexpr unsigned short OMSRCS_VLV_CH_CLOSE( unsigned int v ) { return static_cast<unsigned short>(v + VLV_COUNT); }

	// source of an aft RCS manifold group: its RCS tank, the OMS tanks (interconnect), none (dry manifold), or the other pod's RCS tank (crossfeed)
	enum AFTFEED { AFTFEED_RCS, AFTFEED_OMS, AFTFEED_NONE, AFTFEED_XRCS };
}


#endif// __OMSRCS_INTERFACE_H_
