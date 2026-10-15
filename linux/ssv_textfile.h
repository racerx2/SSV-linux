/****************************************************************************
  This file is part of Space Shuttle Vessel

  Text file input as on Windows (Linux port)
  Modified by racerx2, 2026


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
#ifndef _SSV_TEXTFILE_H_
#define _SSV_TEXTFILE_H_

#include <fstream>
#include <sstream>
#include <string>
#include <iterator>
#include <OrbiterAPI.h>

// an Orbiter-relative text file read like a Windows text-mode ifstream: '\' paths in any letter case, CRLF read as LF
class TextFileIn : public std::istringstream
{
public:
	TextFileIn() = default;
	explicit TextFileIn( const char* path ) { open( path ); }

	void open( const char* path )
	{
		std::ifstream f( oapiResolvePath( path ), std::ios::binary );
		opened = f.is_open();
		std::string s;
		if (opened) s.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );
		std::string t;
		t.reserve( s.size() );
		for (size_t i = 0; i < s.size(); i++)
			if (!(s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')) t += s[i];
		str( t );
		clear();
		if (!opened) setstate( std::ios::failbit );
	}
	bool is_open() const { return opened; }
	void close() { opened = false; }

private:
	bool opened = false;
};

#endif// _SSV_TEXTFILE_H_
