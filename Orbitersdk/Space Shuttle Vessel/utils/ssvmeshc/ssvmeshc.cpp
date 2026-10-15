/****************************************************************************
  This file is part of Space Shuttle Vessel

  ssvmeshc definition


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
2020/04/07   GLS
2020/05/07   GLS
2020/06/20   GLS
2020/07/25   GLS
2021/06/28   GLS
2021/08/24   GLS
2021/09/20   GLS
2021/10/23   GLS
2021/12/25   GLS
2021/12/29   GLS
2021/12/30   GLS
2022/06/29   GLS
2022/08/05   GLS
2022/09/29   GLS
2022/09/30   GLS
2022/10/23   GLS
2022/11/09   GLS
2022/11/29   GLS
2026/10/15   racerx2
********************************************/

#include <stdio.h>
#ifdef _WIN32// not upstream: tchar.h is Windows only
#include <tchar.h>
#endif// not upstream: as above
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <set>
#include <iomanip>
#include <clocale>// not upstream: setlocale
#include <cstdlib>// not upstream: mbstowcs, wcstombs
#include <cstring>// not upstream: strlen


const std::string SSVMESHC_VERSION = "1.2";

#ifndef _WIN32
// not upstream: the user's locale (UTF-8) multibyte strings to wide and back, bytes as they are if invalid
static std::wstring Widen( const char* s )
{
	std::vector<wchar_t> w( strlen( s ) + 1 );
	size_t n = std::mbstowcs( w.data(), s, w.size() );
	if (n == static_cast<size_t>(-1)) return std::wstring( s, s + strlen( s ) );
	return std::wstring( w.data(), n );
}

static std::string Narrow( const std::wstring& ws )
{
	std::vector<char> b( ws.size() * MB_CUR_MAX + 1 );
	size_t n = std::wcstombs( b.data(), ws.c_str(), b.size() );
	if (n == static_cast<size_t>(-1)) return std::string( ws.begin(), ws.end() );
	return std::string( b.data(), n );
}
#endif

std::wstring input_file_name = L"";
std::wstring input_file_namepath = L"";
#ifdef _WIN32
std::wstring output_file_namepath = L".\\meshres.h";
#else
std::wstring output_file_namepath = L"./meshres.h";// not upstream: '/' separator
#endif
std::string group_prefix = "GRP_";
std::string group_suffix = "";
std::string material_prefix = "MAT_";
std::string material_suffix = "";
std::string texture_prefix = "TEX_";
std::string texture_suffix = "";


struct sctGROUP
{
	unsigned int num;
	std::string name;
	unsigned int vertices;
	unsigned int triangles;
};

struct sctMATERIAL 
{
	unsigned int num;
	std::string name;
};

struct sctTEXTURE
{
	unsigned int num;
	std::string name;
};

std::vector<sctGROUP> groups;
std::vector<sctMATERIAL> materials;
std::vector<sctTEXTURE> textures;

const std::string& ToAlphaCode(unsigned long number)
{
	static std::string __result;
	std::string a;
	__result = "";

	if(number == 0)
		return __result;

	while(number>0)
	{
		if(number > 26)
		{
			a = ((char)('A' + (number%26)));
			number /= 26;
		} else {
			a = ((char)('A' + (number - 1)));
			number = 0;
		}

		__result = a + __result;
	}
	return __result;
}

void ParseMesh( bool mat, bool tex )
{
	unsigned int current_group_index = 0;
	std::ifstream mshfile;
	std::string line;
	char buffer[400];
	groups.clear();
#ifdef _WIN32
	mshfile.open(input_file_namepath.c_str());
#else
	mshfile.open(Narrow( input_file_namepath ));// not upstream: no wide-path open on Linux
#endif
	while (mshfile)
	{
		mshfile.getline(buffer, 400);
		line.assign(buffer);
		if (!line.empty() && line.back() == '\r') line.pop_back();// not upstream: CRLF meshes read as in Windows text mode
		if (line.rfind( "LABEL", 0 ) != std::string::npos)
		{
			sctGROUP lg;
			lg.num = current_group_index;
			lg.name = line.substr(6);
			lg.vertices = 0;
			lg.triangles = 0;
			groups.push_back(lg);
		}
		else if (line.rfind( "GEOM", 0 ) != std::string::npos)
		{
			// remove comments
			int comma = line.find( ';' );
			if (comma != line.npos) line[comma] = 0;

			// extract vertex and triangle count
			int space = line.substr( 5 ).find( ' ' );
			groups[current_group_index].vertices = stoi( line.substr( 5, space ) );
			groups[current_group_index].triangles = stoi( line.substr( space + 5 ) );

			current_group_index++;
		}
		else if (mat && (line.rfind( "MATERIALS", 0 ) != std::string::npos))
		{
			unsigned int count = stoi( line.substr( 10 ) );
			for (unsigned int i = 0; mshfile && (i < count); i++)
			{
				mshfile.getline( buffer, 400 );
				line.assign( buffer );
				if (!line.empty() && line.back() == '\r') line.pop_back();// not upstream: as above

				sctMATERIAL tmp;
				tmp.num = i;
				tmp.name = line;
				materials.push_back( tmp );
			}
		}
		else if (tex && (line.rfind( "TEXTURES", 0 ) != std::string::npos))
		{
			unsigned int count = stoi( line.substr( 9 ) );
			for (unsigned int i = 1; mshfile && (i <= count); i++)
			{
				mshfile.getline( buffer, 400 );
				line.assign( buffer );
				if (!line.empty() && line.back() == '\r') line.pop_back();// not upstream: as above

				sctTEXTURE tmp;
				tmp.num = i;
				tmp.name = line;
				textures.push_back( tmp );
			}
		}
	}
	mshfile.close();
}

void WriteHeaderFile( bool mat, bool tex )
{
	std::ofstream hfile;
	std::set<std::string> existing_symbols_groups;
	std::set<std::string> existing_symbols_materials;
	std::set<std::string> existing_symbols_textures;
	existing_symbols_groups.clear();
	existing_symbols_materials.clear();
	existing_symbols_textures.clear();
#ifdef _WIN32
	hfile.open(output_file_namepath.c_str());
#else
	hfile.open(Narrow( output_file_namepath ));// not upstream: as for the mesh
#endif
	
	// create include guard
	std::string guard = std::string( input_file_name.begin(), input_file_name.end() );
	size_t gpos = guard.find_last_of( '.' );
	if (gpos != std::string::npos) guard = guard.substr( 0, gpos );
	for (auto& c : guard) c = toupper( c );
	guard = "_MESH_" + guard + "_H_";
	
	hfile << "// ======================================================" << std::endl;
	hfile << "// Created by ssvmeshc " << SSVMESHC_VERSION << std::endl;
	hfile << "// Input file: " << std::string(input_file_name.begin(), input_file_name.end()) << std::endl;
	hfile << "// ======================================================" << std::endl;
	hfile << std::endl;
	hfile << "#ifndef " << guard << std::endl;
	hfile << "#define " << guard << std::endl;
	hfile << std::endl;
	hfile << "inline constexpr unsigned int NUMGRP" << group_suffix
		<< " = " << groups.size() << ";" << std::endl;
	if (mat) hfile << "inline constexpr unsigned int NUMMAT" << material_suffix
		<< " = " << materials.size() << ";" << std::endl;
	if (tex) hfile << "inline constexpr unsigned int NUMTEX" << texture_suffix
		<< " = " << textures.size() << ";" << std::endl;
	hfile << std::endl;
	for(unsigned int i = 0; i<groups.size(); i++) {

		std::string tmp;
		std::string::iterator iter = groups[i].name.begin();
		tmp = group_prefix;
		while(iter != groups[i].name.end())
		{
			if (isspace(*iter))
			{
				tmp += '_';
			}
			else if (isalnum(*iter))
			{
				tmp += toupper( *iter );
			}
			else if (*iter == '_')
			{
				tmp += '_';
			}
			else if (*iter == '+')
			{
				tmp += "PLUS";
			}
			else if (*iter == '-')
			{
				tmp += "MINUS";
			}
			else
			{
				//ignore
			}
			++iter;
		}

		if(existing_symbols_groups.find(tmp) != existing_symbols_groups.end())
		{
			std::wcout << L"WARNING: repeated group label (" << std::wstring( tmp.begin(), tmp.end() ) << L")" << std::endl;

			std::string temp;
			unsigned long attempt = 0;
			do{
				temp = tmp + ToAlphaCode(attempt);
				attempt++;
			} while(existing_symbols_groups.find(temp) != existing_symbols_groups.end());
			tmp = temp;
		}
		existing_symbols_groups.insert(tmp);

		tmp.append(group_suffix);
		hfile << "inline constexpr unsigned int " << tmp << "\t=\t" << groups[i].num << ";";
		hfile << "\t//" << groups[i].name;
		hfile << std::endl;
	}

	if (mat)
	{
		hfile << std::endl;
		for (unsigned int i = 0; i < materials.size(); i++)
		{
			std::string tmp = material_prefix;
			std::string::iterator iter = materials[i].name.begin();
			while (iter != materials[i].name.end())
			{
				if (isspace( *iter )) tmp += '_';
				else if (isalnum( *iter )) tmp += toupper( *iter );
				else if (*iter == '_') tmp += '_';

				++iter;
			}

			if (existing_symbols_materials.find( tmp ) != existing_symbols_materials.end())
			{
				std::string temp;
				unsigned long attempt = 0;
				do
				{
					temp = tmp + ToAlphaCode( attempt );
					attempt++;
				} while (existing_symbols_materials.find( temp ) != existing_symbols_materials.end() );
				tmp = temp;
			}
			existing_symbols_materials.insert(tmp);

			tmp.append( material_suffix );
			hfile << "inline constexpr unsigned int " << tmp << "\t=\t" << materials[i].num << ";";
			hfile << "\t//" << materials[i].name;
			hfile << std::endl;
		}
	}

	if (tex)
	{
		hfile << std::endl;
		for (unsigned int i = 0; i < textures.size(); i++)
		{
			std::string tmp = texture_prefix;
			std::string tmp2 = textures[i].name;
			tmp2 = tmp2.substr( 0, tmp2.find_last_of( '.' ) );

			std::string::iterator iter = tmp2.begin();
			while (iter != tmp2.end())
			{
				if (isspace( *iter )) tmp += '_';
				else if (isalnum( *iter )) tmp += toupper( *iter );
				else if (*iter == '_') tmp += '_';
				else if (*iter == '/') tmp += '_';
				else if (*iter == '\\') tmp += '_';

				++iter;
			}

			if (existing_symbols_textures.find( tmp ) != existing_symbols_textures.end())
			{
				std::string temp;
				unsigned long attempt = 0;
				do
				{
					temp = tmp + ToAlphaCode( attempt );
					attempt++;
				} while (existing_symbols_textures.find( temp ) != existing_symbols_textures.end() );
				tmp = temp;
			}
			existing_symbols_textures.insert(tmp);

			tmp.append( texture_suffix );
			hfile << "inline constexpr unsigned int " << tmp << "\t=\t" << textures[i].num << ";";
			hfile << "\t//" << textures[i].name;
			hfile << std::endl;
		}
	}

	hfile << std::endl << "#endif// " << guard << std::endl;
	hfile.close();
}

void printhelp()
{
	std::wstring temp;
	temp.assign(SSVMESHC_VERSION.begin(), SSVMESHC_VERSION.end());
	std::wcout << "SSV Mesh labels to C++ converter. Version "
		<< temp << "." << std::endl;
	std::wcout << "Syntax: ssvmeshc [OPTIONS] FILE" << std::endl;
	std::wcout << std::endl;
	std::wcout << "OPTIONS:" << std::endl;
	std::wcout << "   -o FILE     Set output file" << std::endl;
	std::wcout << "   -p PREFIX   Set group symbol prefix" << std::endl;
	std::wcout << "   -s SUFFIX   Set group symbol suffix" << std::endl;
	std::wcout << "   -m	      Enable material list output" << std::endl;
	std::wcout << "   -b PREFIX   Set material symbol prefix" << std::endl;
	std::wcout << "   -n SUFFIX   Set material symbol suffix" << std::endl;
	std::wcout << "   -t	      Enable texture list output" << std::endl;
	std::wcout << "   -e PREFIX   Set texture symbol prefix" << std::endl;
	std::wcout << "   -r SUFFIX   Set texture symbol suffix" << std::endl;
	std::wcout << "   -a          Enable analysis mode (does not support other switches)" << std::endl;
	std::wcout << "   -? -h       Display this help" << std::endl;

}

#ifdef _WIN32
int _tmain(int argc, _TCHAR* argv[])
{
#else
int main(int argc, char* argv_[])// not upstream: no _tmain on Linux
{
	// not upstream: the arguments made wide in the user's locale, as the Unicode _tmain gets them
	setlocale( LC_ALL, "" );
	std::vector<std::wstring> wargs;
	for (int i = 0; i < argc; i++) wargs.push_back( Widen( argv_[i] ) );
	std::vector<const wchar_t*> wargv;
	for (const std::wstring& w : wargs) wargv.push_back( w.c_str() );
	const wchar_t* const* argv = wargv.data();
#endif
	bool bValidInput = false;
	bool doMAT = false;
	bool doTEX = false;
	bool analysis = false;

	if(argc < 1)
	{
		std::wcerr << L"No mesh file given as input." << std::endl;
	}

	for(int i = 1; i<argc; i++)
	{
		if(argv[i][0] == L'-') {
			std::wstring temp;
			//Option
			switch(argv[i][1])
			{
			case L'p':
				temp.assign(argv[i]+2);
				group_prefix = std::string(temp.begin(), temp.end());
				break;
			case L's':
				temp.assign(argv[i]+2);
				group_suffix = std::string(temp.begin(), temp.end());
				break;
			case L'o':
				output_file_namepath.assign(argv[i]+2);
				break;
			case L'm':
				doMAT = true;
				break;
			case L'b':
				if (!doMAT) break;
				temp.assign( argv[i] + 2 );
				material_prefix = std::string( temp.begin(), temp.end() );
				break;
			case L'n':
				if (!doMAT) break;
				temp.assign( argv[i] + 2 );
				material_suffix = std::string( temp.begin(), temp.end() );
				break;
			case L't':
				doTEX = true;
				break;
			case L'e':
				if (!doTEX) break;
				temp.assign( argv[i] + 2 );
				texture_prefix = std::string( temp.begin(), temp.end() );
				break;
			case L'r':
				if (!doTEX) break;
				temp.assign( argv[i] + 2 );
				texture_suffix = std::string( temp.begin(), temp.end() );
				break;
			case L'a':
				analysis = true;
				break;
			case L'?':
			case L'h':
				printhelp();
				return 0;
			default:
				std::wcerr << L"ERROR: Illegal parameter : " << argv[i] << std::endl;
				printhelp();
				return 1;
			}
		} else {
			//std::wcout << "input=" << argv[i] << std::endl;
			input_file_namepath.assign(argv[i]);

#ifdef _WIN32
			size_t pos = input_file_namepath.find_last_of( '\\' );
#else
			size_t pos = input_file_namepath.find_last_of( L"\\/" );// not upstream: '/' separates too
#endif
			if (pos != std::string::npos) input_file_name = input_file_namepath.substr( pos + 1 );
			else input_file_name = input_file_namepath;

			bValidInput = true;
		}
	}

	if(bValidInput)
	{
		if (!analysis)
		{
			std::wcout << L"Input File name: " << input_file_name.c_str() << std::endl;
			std::wcout << L"Output File name: " << output_file_namepath.c_str() << std::endl;
			std::wcout << L"Group prefix: " << std::wstring(group_prefix.begin(), group_prefix.end()) << std::endl;
			std::wcout << L"Group suffix: " << std::wstring(group_suffix.begin(), group_suffix.end()) << std::endl;

			if (doMAT)
			{
				std::wcout << L"Material prefix: " << std::wstring(material_prefix.begin(), material_prefix.end()) << std::endl;
				std::wcout << L"Material suffix: " << std::wstring(material_suffix.begin(), material_suffix.end()) << std::endl;
			}

			if (doTEX)
			{
				std::wcout << L"Texture prefix: " << std::wstring(texture_prefix.begin(), texture_prefix.end()) << std::endl;
				std::wcout << L"Texture suffix: " << std::wstring(texture_suffix.begin(), texture_suffix.end()) << std::endl;
			}

			std::wcout << L"Parse mesh file..." << std::endl;
			ParseMesh( doMAT, doTEX );

			WriteHeaderFile( doMAT, doTEX );

			std::wcout << L"Found:" << std::endl;
			std::wcout << L"\t" << groups.size() << L" groups" << std::endl;
			if (doMAT) std::wcout << L"\t" << materials.size() << L" materials" << std::endl;
			if (doTEX) std::wcout << L"\t" << textures.size() << L" textures" << std::endl;
		}
		else
		{
			std::wcout << L"Input File name: " << input_file_name.c_str() << std::endl;
			std::wcout << L"Mesh analysis mode" << std::endl;

			std::wcout << L"Parse mesh file..." << std::endl;
			ParseMesh( false, false );

			int total_vertices = 0;
			int total_triangles = 0;
			for (unsigned int i = 0; i < groups.size(); i++)
			{
				total_vertices += groups[i].vertices;
				total_triangles += groups[i].triangles;
			}
			std::wcout << L"Found:" << std::endl;
			std::wcout << groups.size() << L" groups" << L" " << std::setw( 40 ) << total_vertices << L" vertices" << L"  " << total_triangles << L" triangles" << std::endl;

			for (unsigned int i = 0; i < groups.size(); i++)
			{
				std::wcout.precision( 2 );
				std::wcout << std::right << std::setw( 4 ) << groups[i].num << L"  " <<
					std::left << std::setw( 40 ) << groups[i].name.c_str() << L"  " <<
					std::fixed << std::right << std::setw( 5 ) << groups[i].vertices << L" " <<
					std::fixed << std::right << std::setw( 6 ) << ((100.0 * groups[i].vertices) / total_vertices) << L"%" << L"  " <<
					std::fixed << std::right << std::setw( 5 ) << groups[i].triangles << L" " <<
					std::fixed << std::right << std::setw( 6 ) << ((100.0 * groups[i].triangles) / total_triangles) << L"%" <<
					std::endl;
			}
		}

		std::wcout << std::endl << L"Done! Bye..." << std::endl;
		return 0;
	}
	else {
		std::wcerr << L"ERROR: No input file specified." << std::endl;
		printhelp();
		return 1;
	}

}
