![SSV logo](./Doc/Space%20Shuttle%20Vessel/Images/logo1000.png)

# Space Shuttle Vessel
Space Shuttle Vessel (SSV) is an addon for Orbiter Space Flight Simulator (https://github.com/orbitersim/orbiter). The purpose of this addon is to simulate NASA’s Space Transportation System Program as much as possible. Currently only some elements have been completed and work on others is ongoing.

![Launch](./Doc/Space%20Shuttle%20Vessel/Images/launch.png)
![Upper stage being deployed in orbit](./Doc/Space%20Shuttle%20Vessel/Images/orbit.png)
![Landing viewed from the HUD](./Doc/Space%20Shuttle%20Vessel/Images/landing.png)

This release of SSV is for use in Orbiter 2024.


# Linux (unofficial port)
This fork (branch `linux`) builds SSV for the native Linux Orbiter (https://github.com/racerx2/orbiter-2027-linux) and its Vulkan client. It is not supported by the SSV team. Every Linux change in the code is marked `not upstream`; the Windows code stays under `#ifdef _WIN32`.

Install: extract `SSV-1.16-linux-1.0-x64.zip` into the Orbiter folder, then run `python3 "Install/Space Shuttle Vessel/ssv_linux_setup.py"` from it once. See `linux/SSV-Linux.txt` (in the zip: `Doc/Space Shuttle Vessel/SSV-Linux.txt`).

Build (CMake 3.19+, Ninja, GCC, Qt 6): `cmake -S . -B out/build -G Ninja -DORBITER_ROOT=<Orbiter folder> -DORBITER_CMAKE_DIR=<Orbiter source>/cmake`, then `cmake --build out/build`. The sources and the mesh and COMPOOL header generators come from the Visual Studio projects (`linux/vcxproj.py`). `linux/mkpkg.py out/build <manual.pdf|-> out` makes the zip.

## Screenshots
In the Orbiter Linux Vulkan client: plumes, entry heating, the OPS 1 aborts this port adds, the HUD.

| | |
|---|---|
| ![STS-26R liftoff from LC-39B, SRB and SSME plumes](linux/screenshots/launch-39b.jpg)<br>STS-26R liftoff from LC-39B, SRB and SSME plumes | ![Clearing the tower](linux/screenshots/launch-plumes.jpg)<br>Clearing the tower |
| ![T+35 s: satin paint on the SRBs](linux/screenshots/srb-paint.jpg)<br>T+35 s: satin paint on the SRBs | ![T+110 s, plumes spreading in thin air](linux/screenshots/ascent.jpg)<br>T+110 s, plumes spreading in thin air |
| ![SRB separation](linux/screenshots/srb-separation.jpg)<br>SRB separation | ![OPS 1 abort to orbit: ATO TRAJ 2, MM 103 ATO](linux/screenshots/ops-ato-traj.jpg)<br>OPS 1 abort to orbit: ATO TRAJ 2, MM 103 ATO |
| ![ATO 1 MNVR EXEC, the abort OMS-1 target loaded](linux/screenshots/ops-ato-oms1.jpg)<br>ATO 1 MNVR EXEC, the abort OMS-1 target loaded | ![Abort once around: AOA MNVR TRANS, OPS 301 flashing](linux/screenshots/ops-aoa-ops301.jpg)<br>Abort once around: AOA MNVR TRANS, OPS 301 flashing |
| ![Entry heating, STS-107 entry interface](linux/screenshots/reentry-heating.jpg)<br>Entry heating, STS-107 entry interface | ![The nose cap and belly at peak heating](linux/screenshots/reentry-nose.jpg)<br>The nose cap and belly at peak heating |
| ![Entry over land in daylight](linux/screenshots/reentry-day.jpg)<br>Entry over land in daylight | ![KSC 15 approach, HUD on (MM 305)](linux/screenshots/hud-approach.jpg)<br>KSC 15 approach, HUD on (MM 305) |
| ![The HUD combiners glow through the windshield while HUD POWER is on](linux/screenshots/hud-glass-glow.jpg)<br>The HUD combiners glow through the windshield while HUD POWER is on |  |


# Installation
1. Install Orbiter 2024.

2. Install the required addons:

	Antelope Valley scenery pack (https://www.orbiter-forum.com/resources/orbiter-2016-high-resolution-texture-packs.5676/)

3. Extract the SSV files into your Orbiter installation folder, overwriting any existing files.
WARNING: The SSV installation overwrites the default Base.cfg and Earth.cfg files.

4. Install the "SSV_Font_A" and "SSV_Font_B" fonts, located in the "<Orbiter installation>\Install\Space Shuttle Vessel" directory, by opening them and selecting "Install". After successful installation the files can be deleted.

5. The displays in SSV require the MFD resolution of 512 x 512 (Orbiter Launchpad > Extra > Instruments and panels > MFD parameter configuration > MFD texture size).

6. The local light sources have to be enabled for the external lights in SSV to work (Orbiter Launchpad > Visual effects > Local light sources, and additionally in D3D9 Orbiter Launchpad > Video > Advanced > Local lights).

7. For a better visual experience, using the D3D9 graphics client is strongly recommended, although not required. If using the D3D9 graphics client, the "Disable near clip plane compatibility mode" option in the D3D9 Advanced Setup dialog (Orbiter Launchpad > Video > Advanced) should be checked.

NOTE: If you encounter the error "msvcp140.dll is missing" you need to download the Microsoft Visual C++ Redistributable for Visual Studio 2017.


## Optional addons
It is recommended the installation of the excellent Shuttle FDO MFD (https://github.com/indy91/Shuttle-FDO-MFD) by indy91, which performs the calculations done by the Flight Dynamics Officers in Mission Control, allowing an accurate rendezvous profile simulation, as well as targeting a deorbit burn to land at a chosen runway.


## Known issues
A compatibility issue has been identified with the addon "Attachment Manager", so it might have to be disabled for SSV to work properly.


# Credits
Space Shuttle Vessel is based on revision 3242 of Space Shuttle Ultra (svn://orbiter-radio.co.uk/shuttleultra).

The SSV vessels load JSON files with cJSON library by Dave Gamble and cJSON contributors (https://github.com/DaveGamble/cJSON), included in the code. The Mission Editor loads and saves JSON files with Json.NET by James Newton-King (https://www.newtonsoft.com/json).

Large parts of the launch autopilot were copied (with minor modifications) from PEG MFD.

Some of the attitude control code was derived from Attitude MFD V3.

Vandenberg base uses part of the VandenbergAFB-2006 addon (https://www.orbiter-forum.com/resources/vandenbergafb-2006.3523/) by Usonian.

The SurfaceRoving class is based on the GeneralVehicle addon (https://www.orbiter-forum.com/resources/generalvehicle.3158/) by Fred18.

The file "circuit_breaker.wav" comes from Project Apollo - NASSP (https://github.com/orbiternassp/NASSP).


# License
This addon is open-source and is released under the GNU GPL v2 (see file [SSV-LICENSE.txt](SSV-LICENSE.txt) for details).


# Disclaimer
The SSV team is not responsible for any crashes or other problems caused by this addon. Use at your own risk.
