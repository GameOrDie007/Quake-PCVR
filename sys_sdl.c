
#ifdef WIN32
#ifdef _MSC_VER
#pragma comment(lib, "sdl.lib")
#pragma comment(lib, "sdlmain.lib")
#endif
#include <io.h>
#include "conio.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

#include <signal.h>

#include <SDL.h>

#include "quakedef.h"
#include "vr_tbxr.h"
#include "vr_pc.h"

extern int vr_eyewidth;
extern int vr_eyeheight;

// =======================================================================
// General routines
// =======================================================================

void Sys_Shutdown (void)
{
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif
	fflush(stdout);
	SDL_Quit();
}


void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[MAX_INPUTLINE];

// change stdin to non blocking
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

	va_start (argptr,error);
	dpvsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Con_Printf ("Quake Error: %s\n", string);

	Host_Shutdown ();
	exit (1);
}

static int outfd = 1;
void Sys_PrintToTerminal(const char *text)
{
	if(outfd < 0)
		return;
#ifdef FNDELAY
	// BUG: for some reason, NDELAY also affects stdout (1) when used on stdin (0).
	// this is because both go to /dev/tty by default!
	{
		int origflags = fcntl (outfd, F_GETFL, 0);
		fcntl (outfd, F_SETFL, origflags & ~FNDELAY);
#endif
#ifdef WIN32
#define write _write
#endif
		while(*text)
		{
			fs_offset_t written = (fs_offset_t)write(outfd, text, strlen(text));
			if(written <= 0)
				break; // sorry, I cannot do anything about this error - without an output
			text += written;
		}
#ifdef FNDELAY
		fcntl (outfd, F_SETFL, origflags);
	}
#endif
	//fprintf(stdout, "%s", text);
}

char *Sys_ConsoleInput(void)
{
//	if (cls.state == ca_dedicated)
	{
		static char text[MAX_INPUTLINE];
		int len = 0;
#ifdef WIN32
		int c;

		// read a line out
		while (_kbhit ())
		{
			c = _getch ();
			_putch (c);
			if (c == '\r')
			{
				text[len] = 0;
				_putch ('\n');
				len = 0;
				return text;
			}
			if (c == 8)
			{
				if (len)
				{
					_putch (' ');
					_putch (c);
					len--;
					text[len] = 0;
				}
				continue;
			}
			text[len] = c;
			len++;
			text[len] = 0;
			if (len == sizeof (text))
				len = 0;
		}
#else
		fd_set fdset;
		struct timeval timeout;
		FD_ZERO(&fdset);
		FD_SET(0, &fdset); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select (1, &fdset, NULL, NULL, &timeout) != -1 && FD_ISSET(0, &fdset))
		{
			len = read (0, text, sizeof(text));
			if (len >= 1)
			{
				// rip off the \n and terminate
				text[len-1] = 0;
				return text;
			}
		}
#endif
	}
	return NULL;
}

char *Sys_GetClipboardData (void)
{
#ifdef WIN32
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard (NULL) != 0)
	{
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData (CF_TEXT)) != 0)
		{
			if ((cliptext = (char *)GlobalLock (hClipboardData)) != 0)
			{
				size_t allocsize;
				allocsize = GlobalSize (hClipboardData) + 1;
				data = (char *)Z_Malloc (allocsize);
				strlcpy (data, cliptext, allocsize);
				GlobalUnlock (hClipboardData);
			}
		}
		CloseClipboard ();
	}
	return data;
#else
	return NULL;
#endif
}

void Sys_InitConsole (void)
{
}

/*
	Restart the process with a different game selected.

	Changing gamedir in the engine ends in vid_restart, which destroys the GL
	context - and the OpenXR swapchain images are textures owned by that
	context, so the session goes with it and the headset is left behind. Their
	build never meets this: on Android every game is its own launcher intent
	and the process always starts fresh. A mission pack also needs its game
	mode, which is read once at startup, so a restart is the only way to get
	-hipnotic and -rogue right anyway.

	The current command line is reused, so -basedir, -nohome and anything else
	the launcher passed survive; only the game selection is replaced.
*/
void Host_RelaunchGame_f (void)
{
#ifdef WIN32
	char exe[MAX_OSPATH];
	char cmdline[8192];
	char pidarg[64];
	int i;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	if (!GetModuleFileNameA(NULL, exe, sizeof(exe)))
	{
		Con_Printf("relaunchgame: cannot find my own executable\n");
		return;
	}

	strlcpy(cmdline, "\"", sizeof(cmdline));
	strlcat(cmdline, exe, sizeof(cmdline));
	strlcat(cmdline, "\"", sizeof(cmdline));

	// Everything the launcher passed, minus whatever picked a game.
	for (i = 1; i < com_argc; i++)
	{
		const char *a = com_argv[i];

		if (!a || !a[0])
			continue;
		if (a[0] == '+')
		{
			// A + command belongs to the launch that was given it: re-running
			// it against a different game would be wrong, and a +relaunchgame
			// would hand the new instance the same instruction forever. Skip
			// the command and its arguments, which run to the next switch.
			while (i + 1 < com_argc && com_argv[i + 1][0] != '-' && com_argv[i + 1][0] != '+')
				i++;
			continue;
		}
		if (!strcasecmp(a, "-game") || !strcasecmp(a, "-relaunchwait") ||
			!strcasecmp(a, "-spmenu"))
		{
			i++;  // and the value that follows it
			continue;
		}
		if (!strcasecmp(a, "-quake") || !strcasecmp(a, "-hipnotic") ||
			!strcasecmp(a, "-rogue") || !strcasecmp(a, "-nehahra") ||
			!strcasecmp(a, "-quoth"))
			continue;

		strlcat(cmdline, " ", sizeof(cmdline));
		if (strchr(a, ' '))
		{
			strlcat(cmdline, "\"", sizeof(cmdline));
			strlcat(cmdline, a, sizeof(cmdline));
			strlcat(cmdline, "\"", sizeof(cmdline));
		}
		else
			strlcat(cmdline, a, sizeof(cmdline));
	}

	// The new selection, already in command line form. Quoted the same way as
	// the inherited arguments above, because a mod directory may contain a
	// space and would otherwise arrive as two arguments.
	for (i = 1; i < Cmd_Argc(); i++)
	{
		const char *a = Cmd_Argv(i);

		strlcat(cmdline, " ", sizeof(cmdline));
		if (strchr(a, ' '))
		{
			strlcat(cmdline, "\"", sizeof(cmdline));
			strlcat(cmdline, a, sizeof(cmdline));
			strlcat(cmdline, "\"", sizeof(cmdline));
		}
		else
			strlcat(cmdline, a, sizeof(cmdline));
	}

	// The runtime will not hand the new instance a session while this one
	// still holds it, so the new instance is told to wait for this process to
	// exit before it asks. See -relaunchwait in main.
	dpsnprintf(pidarg, sizeof(pidarg), " -relaunchwait %u", (unsigned)GetCurrentProcessId());
	strlcat(cmdline, pidarg, sizeof(cmdline));

	// Relaunches come from the game list and from the mod browser, and in both
	// cases the player has just chosen something to play, so the new instance
	// opens on Single Player rather than on the credits.
	strlcat(cmdline, " -spmenu", sizeof(cmdline));

	/*
		Carry the VR settings across explicitly.

		They are archived, but DarkPlaces writes config.cfg into the *current*
		gamedir - so hipnotic, rogue and each MachineGames pack keep their own
		copy, and a relaunch into one of them comes up with whatever that copy
		says rather than what the player just chose. On top of that the loop
		above deliberately drops every + command, which is correct for the ones
		a launcher passed and wrong for these.

		This is why switching to an expansion from the game list arrived with
		menus-in-world off while plain Quake had it on.
	*/
	{
		static const char * const carry[] = {
			"vr_menu_in_world",
			"vr_menu_in_world_dim",
			"vr_menu_in_world_scale",
			"vr_hud_height",
			NULL
		};
		int c;

		for (c = 0; carry[c]; c++)
		{
			cvar_t *v = Cvar_FindVar(carry[c]);

			if (!v)
				continue;

			// One argv element, quoted, so the command and its value arrive
			// together the way the engine expects.
			strlcat(cmdline, " \"+", sizeof(cmdline));
			strlcat(cmdline, v->name, sizeof(cmdline));
			strlcat(cmdline, " ", sizeof(cmdline));
			strlcat(cmdline, v->string, sizeof(cmdline));
			strlcat(cmdline, "\"", sizeof(cmdline));
		}
	}

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));

	Con_Printf("relaunching: %s\n", cmdline);

	if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		Con_Printf("relaunchgame: could not start a new instance (error %u)\n",
				   (unsigned)GetLastError());
		return;
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	// Quit the ordinary way, so the session is torn down before the context.
	Cbuf_AddText("quit\n");
#else
	Con_Printf("relaunchgame: only implemented on Windows\n");
#endif
}

/*
	Wait for the instance that spawned this one to exit, so that the OpenXR
	runtime is free before the session is created. Five seconds is far longer
	than a shutdown takes; if it expires anyway, carrying on is better than
	hanging on the splash screen.
*/
static void Sys_WaitForRelaunch (void)
{
#ifdef WIN32
	int i = COM_CheckParm("-relaunchwait");
	HANDLE parent;

	if (!i || i + 1 >= com_argc)
		return;

	parent = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)atoi(com_argv[i + 1]));
	if (!parent)
		return;  // already gone, which is the common case

	WaitForSingleObject(parent, 5000);
	CloseHandle(parent);

	// The process is gone, but the runtime may still be letting go of the
	// session it held. A short cushion here costs nothing and is cheaper
	// than falling back to flatscreen because the session was refused.
	Sleep(400);
#endif
}

int main (int argc, char *argv[])
{
	signal(SIGFPE, SIG_IGN);

	com_argc = argc;
	com_argv = (const char **)argv;
	Sys_ProvideSelfFD();

	// COMMANDLINEOPTION: sdl: -noterminal disables console output on stdout
	if(COM_CheckParm("-noterminal"))
		outfd = -1;
	// COMMANDLINEOPTION: sdl: -stderr moves console output to stderr
	else if(COM_CheckParm("-stderr"))
		outfd = 2;
	else
		outfd = 1;

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	// If this instance was started by another one to change game, the old
	// one has to be gone before OpenXR will hand this one a session.
	Sys_WaitForRelaunch();

	// we don't know which systems we'll want to init, yet...
	SDL_Init(0);

	/*
		OpenXR comes up in two halves, which their build has no need to do.
		The instance and the eye resolution need no graphics, so they come
		first and the engine can size itself to one eye buffer the way their
		Android build does. The session must wait for the GL context SDL
		creates inside Host_Init.
	*/
	// COMMANDLINEOPTION: vr: -novr skips OpenXR entirely and runs flatscreen
	if (!COM_CheckParm("-novr") && TBXR_InitialiseInstance())
		TBXR_GetEyeResolution(&vr_eyewidth, &vr_eyeheight);

	// Their host.c reduces Host_Main to Host_Init, because on Android the app
	// thread owns the frame loop and calls into the engine. VR_MainLoop is
	// that loop's PC counterpart and does not return.
	Host_Main();

	// Whatever the instance phase had to say, said now that it can be read.
	VR_FlushEarlyLog();

	if (vr_eyewidth > 0 && !VR_Startup())
	{
		Con_Printf("VR: startup failed, continuing flatscreen\n");
	}

	// COMMANDLINEOPTION: vr: -spmenu opens Single Player once the game is up
	// (see Host_RelaunchGame_f, which is the only thing that passes it)
	if (COM_CheckParm("-spmenu"))
		Cbuf_AddText("menu_singleplayer\n");

	VR_MainLoop();

	return 0;
}

qboolean sys_supportsdlgetticks = true;
unsigned int Sys_SDL_GetTicks (void)
{
	return SDL_GetTicks();
}
void Sys_SDL_Delay (unsigned int milliseconds)
{
	SDL_Delay(milliseconds);
}
