@echo off
rem Prepare this folder to play.
rem
rem Copies Quake, any expansions and the soundtrack out of your own install,
rem then builds the menu artwork and the message text for the MachineGames
rem episodes. Everything it produces is made here, from game data already on
rem this machine - none of it is distributed.
rem
rem Set QQ_QUAKEDIR first if Quake is somewhere this cannot guess.

cd /d "%~dp0"

set QQPY=
where python >nul 2>&1 && set QQPY=python
if not defined QQPY (
	where py >nul 2>&1 && set QQPY=py
)

if not defined QQPY (
	echo.
	echo Python 3 is needed to prepare the install, and was not found.
	echo Get it from https://www.python.org/downloads/ - tick
	echo "Add python.exe to PATH" while installing - then run this again.
	echo.
	pause
	exit /b 1
)

%QQPY% tools\setup.py .

echo.
pause
