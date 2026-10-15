# Space Shuttle Vessel - Linux port, modified by racerx2, 2026
# Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt)

# not upstream: Linux build of SSV for the native Linux Orbiter; the Visual Studio projects stay the source of truth (linux/vcxproj.py)
cmake_minimum_required(VERSION 3.19)
project(SpaceShuttleVessel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release)
endif()

# same as Orbiter's own modules
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
add_compile_options(-Wformat)
# Orbiter's run path: Modules, Modules/<sub>'s parent, the Orbiter folder (lua.so), also in the build tree
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN" "$ORIGIN/.." "$ORIGIN/../..")

set(ORBITER_ROOT "" CACHE PATH "Orbiter folder (Modules/, Orbitersdk/, lua.so)")
set(ORBITER_CMAKE_DIR "${ORBITER_ROOT}/cmake" CACHE PATH "folder with Orbiter's rcresources.cmake and rc2cpp.py")
if(NOT EXISTS "${ORBITER_ROOT}/Orbitersdk/include/OrbiterAPI.h")
	message(FATAL_ERROR "ORBITER_ROOT must be the Orbiter folder (with Orbitersdk/include)")
endif()
if(NOT EXISTS "${ORBITER_CMAKE_DIR}/rcresources.cmake")
	message(FATAL_ERROR "ORBITER_CMAKE_DIR must hold rcresources.cmake and rc2cpp.py")
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${ORBITER_ROOT}" CACHE PATH "" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SDK_DIR ${ORBITER_ROOT}/Orbitersdk)
set(ORBITER_SOURCE_SDK_INCLUDE_DIR ${SDK_DIR}/include) # rc2cpp include path
include(${ORBITER_CMAKE_DIR}/rcresources.cmake)

set(SSV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Orbitersdk/Space Shuttle Vessel")

# the projects, their sources and the generated headers, read from the .vcxproj files at configure time
file(GLOB_RECURSE SSV_VCXPROJ CONFIGURE_DEPENDS "${SSV_DIR}/*.vcxproj")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SSV_VCXPROJ} ${CMAKE_CURRENT_SOURCE_DIR}/linux/vcxproj.py)
execute_process(
	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/linux/vcxproj.py ${SSV_DIR} ${CMAKE_CURRENT_BINARY_DIR}/projects.cmake
	RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
	message(FATAL_ERROR "linux/vcxproj.py failed")
endif()
include(${CMAKE_CURRENT_BINARY_DIR}/projects.cmake)

# the MSVC secure C library functions (C11 Annex K), force-included into the C++ sources
add_library(ssv_crt STATIC linux/ssv_crt.cpp)
target_include_directories(ssv_crt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/linux)
set(SSV_CRT_FLAGS "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/linux/ssv_crt.h>")

# build tools: mesh group headers and the COMPOOL header, as the meshres and COMPOOL projects make them on Windows
foreach(tool ssvmeshc HALS2CPP)
	add_executable(${tool} ${SSV_${tool}_SRC})
	target_compile_options(${tool} PRIVATE ${SSV_CRT_FLAGS})
	target_link_libraries(${tool} PRIVATE ssv_crt)
endforeach()
set(SSV_GENERATED)
foreach(cmd ${SSV_GEN_ssvmeshc})
	string(REPLACE "|" ";" argv "${cmd}")
	list(GET argv 0 input)
	set(output)
	foreach(a ${argv})
		if(a MATCHES "^-o(.*)")
			set(output "${CMAKE_MATCH_1}")
		endif()
	endforeach()
	add_custom_command(OUTPUT "${output}" COMMAND ssvmeshc ${argv} DEPENDS "${input}" ssvmeshc VERBATIM)
	list(APPEND SSV_GENERATED "${output}")
endforeach()
foreach(cmd ${SSV_GEN_HALS2CPP})
	string(REPLACE "|" ";" argv "${cmd}")
	list(GET argv 0 input)
	list(GET argv 1 output)
	add_custom_command(OUTPUT "${output}" COMMAND HALS2CPP ${argv} DEPENDS "${input}" HALS2CPP VERBATIM)
	list(APPEND SSV_GENERATED "${output}")
endforeach()
add_custom_target(ssv_generated DEPENDS ${SSV_GENERATED})

# libraries and vessel modules
set(SSV_INCLUDES ${SDK_DIR}/include ${SDK_DIR}/XRSound "${SSV_DIR}/SSVlib/include" "${SSV_DIR}/cJSON/inc")
foreach(p ${SSV_PROJECTS})
	if(p STREQUAL "ssvmeshc" OR p STREQUAL "HALS2CPP")
		continue()
	endif()
	if(SSV_${p}_TYPE STREQUAL "StaticLibrary")
		add_library(${p} STATIC ${SSV_${p}_SRC})
	elseif(SSV_${p}_TYPE STREQUAL "DynamicLibrary")
		add_library(${p} SHARED ${SSV_${p}_SRC})
		foreach(rcfile ${SSV_${p}_RC})
			orbiter_rc_resources(${p} "${rcfile}")
		endforeach()
		target_link_libraries(${p} PRIVATE
			${SSV_${p}_REFS}
			${SDK_DIR}/lib/libOrbitersdk.a
			${SDK_DIR}/lib/libDlgCtrl.a
			${SDK_DIR}/XRSound/libXRSound.a
			Qt6::Widgets
		)
		install(TARGETS ${p} LIBRARY DESTINATION Modules)
	else()
		continue()
	endif()
	target_include_directories(${p} PRIVATE "${SSV_${p}_DIR}" ${SSV_INCLUDES})
	target_compile_options(${p} PRIVATE ${SSV_CRT_FLAGS})
	target_link_libraries(${p} PRIVATE ssv_crt)
	add_dependencies(${p} ssv_generated)
endforeach()
# the VAB's Lua functions use Orbiter's Lua 5.1, as the stock DeltaGlider does
target_link_libraries(SSV_VAB PRIVATE ${ORBITER_ROOT}/lua.so)

# tests of the C library functions
enable_testing()
add_executable(ssv_crt_test linux/tests/crt_test.cpp)
target_link_libraries(ssv_crt_test PRIVATE ssv_crt)
add_test(NAME ssv_crt COMMAND ssv_crt_test)
