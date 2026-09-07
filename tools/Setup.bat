@echo off
rem Prepare this folder to play.
rem
rem Copies Quake, any expansions and the soundtrack out of your own install,
rem then builds the menu artwork and the message text for the MachineGames
rem episodes. Everything it produces is made here, from game data already on
rem this machine - none of it is distributed.
rem
rem Nothing has to be installed first. This runs on the PowerShell that comes
rem with Windows, and reads your Quake from this machine - there is no download
rem and no other dependency.
rem
rem Set QQ_QUAKEDIR first if Quake is somewhere this cannot guess.

cd /d "%~dp0"

rem -ExecutionPolicy Bypass applies to this one run only. It changes no system
rem setting, and is what lets a downloaded script run without the user having to
rem alter anything.
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\setup.ps1" "."

if errorlevel 1 goto failed
echo.
pause
exit /b 0

:failed
echo.
echo Setup did not finish. The messages above say why.
echo.
pause
exit /b 1
