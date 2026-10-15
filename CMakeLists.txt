# Space Shuttle Vessel - Linux port, modified by racerx2, 2026
# Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt)

# not upstream: Linux build of SSV for the native Linux Orbiter; the Visual Studio projects stay the source of truth (linux/vcxproj.py)
cmake_minimum_required(VERSION 3.19)
project(SpaceShuttleVessel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
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
# Orbiter's run path: the modules go to Modules/ and link only lua.so from the Orbiter folder, also in the build tree
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN/..")

set(ORBITER_ROOT "" CACHE PATH "Orbiter folder (Modules/, Orbitersdk/, lua.so)")
if(NOT EXISTS "${ORBITER_ROOT}/Orbitersdk/include/OrbiterAPI.h")
	message(FATAL_ERROR "ORBITER_ROOT must be the Orbiter folder (with Orbitersdk/include)")
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${ORBITER_ROOT}" CACHE PATH "" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Orbiter's CMake package: the SDK libraries, lua.so and orbiter_rc_resources(), only from ORBITER_ROOT
if(POLICY CMP0144)
	cmake_policy(SET CMP0144 NEW) # ORBITER_ROOT is also Orbiter's <PACKAGENAME>_ROOT: NO_DEFAULT_PATH ignores it
endif()
find_package(Orbiter CONFIG REQUIRED HINTS "${ORBITER_ROOT}/Orbitersdk/lib/cmake" NO_DEFAULT_PATH)
file(REAL_PATH "${ORBITER_ROOT}" orbiter_root)
file(REAL_PATH "${Orbiter_ROOT_DIR}" orbiter_found)
if(NOT orbiter_root STREQUAL orbiter_found)
	message(FATAL_ERROR "Orbiter_DIR is the package of ${Orbiter_ROOT_DIR}, not of ORBITER_ROOT: remove it from the cache")
endif()
set(SDK_INCLUDE "$<TARGET_PROPERTY:Orbiter::Orbitersdk,INTERFACE_INCLUDE_DIRECTORIES>")

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
set(SSV_CRT_FLAGS "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/linux/ssv_crt.h\">")

# build tools: mesh group headers and the COMPOOL header, as the meshres and COMPOOL projects make them on Windows (no secure C library calls)
foreach(tool ssvmeshc HALS2CPP)
	add_executable(${tool} ${SSV_${tool}_SRC})
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

# libraries and vessel modules; a module links the libraries of its vcxproj (SSV_<name>_LIBS)
set(SSV_INCLUDES ${SDK_INCLUDE} "${SSV_DIR}/SSVlib/include" "${SSV_DIR}/cJSON/inc")
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
			${SSV_${p}_LIBS}
			Qt6::Widgets # the SDK dialog headers
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

# tests of the C library functions
enable_testing()
add_executable(ssv_crt_test linux/tests/crt_test.cpp)
target_link_libraries(ssv_crt_test PRIVATE ssv_crt)
add_test(NAME ssv_crt COMMAND ssv_crt_test)

# tests of the flight recorder support (design ssv-flightrec), Orbiter's recorder calls stubbed in the test
add_executable(ssv_flightrec_test linux/tests/flightrec_test.cpp "${SSV_DIR}/SSVlib/src/FlightRecorder.cpp" "${SSV_DIR}/OV/CrewControls.cpp")
target_include_directories(ssv_flightrec_test PRIVATE ${SDK_INCLUDE} "${SSV_DIR}/SSVlib/include" "${SSV_DIR}/OV")
target_compile_options(ssv_flightrec_test PRIVATE ${SSV_CRT_FLAGS})
target_link_libraries(ssv_flightrec_test PRIVATE ssv_crt)
add_test(NAME ssv_flightrec COMMAND ssv_flightrec_test)
