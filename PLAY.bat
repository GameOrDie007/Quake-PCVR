@echo off
rem QuakeQuest PCVR - test launcher.
rem
rem Start Virtual Desktop on the headset and connect it FIRST, so VDXR is the
rem running OpenXR runtime, then run this.
rem
rem Pass -novr to force the flatscreen build:  PLAY.bat -novr

cd /d "%~dp0"
start "" "%~dp0darkplaces-sdl.exe" -basedir run -condebug %*
