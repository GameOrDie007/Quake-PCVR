#include "quakedef.h"

// Radial weapon selector. The VR layer (QuakeQuest_OpenXR.c) holds the grip, freezes the wheel
// plane and drives the cursor. This file owns the weapon list, the ring geometry, the hit test,
// the models and the drawing.

cvar_t vr_weaponwheel = {CVAR_SAVE, "vr_weaponwheel", "1", "enable the radial weapon selector on the dominant hand grip"};
cvar_t vr_weaponwheel_distance = {CVAR_SAVE, "vr_weaponwheel_distance", "0.35", "distance in metres from the hand to the centre of the weapon wheel"};
cvar_t vr_weaponwheel_radius = {CVAR_SAVE, "vr_weaponwheel_radius", "0.2", "radius in metres of the weapon wheel"};
cvar_t vr_weaponwheel_modelsize = {CVAR_SAVE, "vr_weaponwheel_modelsize", "0.11", "size in metres of the weapon models on the wheel"};
cvar_t vr_weaponwheel_modelpitch = {CVAR_SAVE, "vr_weaponwheel_modelpitch", "20", "pitch of the weapon models on the wheel"};
cvar_t vr_weaponwheel_modelyaw = {CVAR_SAVE, "vr_weaponwheel_modelyaw", "-145", "yaw of the weapon models on the wheel, relative to the wheel"};
cvar_t vr_weaponwheel_spin = {CVAR_SAVE, "vr_weaponwheel_spin", "45", "degrees per second that the weapon models turn on the wheel, 0 stops them"};
cvar_t vr_weaponwheel_deflection = {CVAR_SAVE, "vr_weaponwheel_deflection", "22.5", "degrees of controller rotation needed to move the cursor to the edge of the wheel"};
cvar_t vr_weaponwheel_slowmo = {CVAR_SAVE, "vr_weaponwheel_slowmo", "0.3", "game speed while the weapon wheel is open, 1 disables the slow down"};
cvar_t vr_weaponwheel_scan = {0, "vr_weaponwheel_scan", "0", "print the item bits, the active weapon and the view model whenever they change, to help write a weaponwheel.json section for a mod"};

// written by the VR layer
int weaponwheel_active;
float weaponwheel_angles[3];
float weaponwheel_cursor[2];
// written here, read by the VR layer for haptics
int weaponwheel_selection = -1;

extern float gunorg[3];
extern cvar_t vr_worldscale;
extern cvar_t cl_righthanded;
extern cvar_t slowmo;

void R_DrawBLineMesh(vec3_t mins, vec3_t maxs, float thickness, float cr, float cg, float cb, float ca);
void TBXR_Vibrate(int duration, int chan, float intensity);

#define WEAPONWHEEL_CONFIG "weaponwheel.json"
#define MAX_WHEEL_SLOTS 16

typedef struct wheelweapon_s
{
	int itembit;
	int impulse;
	char modelname[MAX_QPATH];
	char name[64];
}
wheelweapon_t;

// used when weaponwheel.json is missing or unreadable. The impulse and item bit values agree
// with IN_BestWeapon_ResetData in cl_input.c.
static const wheelweapon_t wheelweapons_builtin[] =
{
	{IT_AXE,				1, "progs/v_axe.mdl",	"Axe"},
	{IT_SHOTGUN,			2, "progs/v_shot.mdl",	"Shotgun"},
	{IT_SUPER_SHOTGUN,		3, "progs/v_shot2.mdl",	"Super Shotgun"},
	{IT_NAILGUN,			4, "progs/v_nail.mdl",	"Nailgun"},
	{IT_SUPER_NAILGUN,		5, "progs/v_nail2.mdl",	"Super Nailgun"},
	{IT_GRENADE_LAUNCHER,	6, "progs/v_rock.mdl",	"Grenade Launcher"},
	{IT_ROCKET_LAUNCHER,	7, "progs/v_rock2.mdl",	"Rocket Launcher"},
	{IT_LIGHTNING,			8, "progs/v_light.mdl",	"Thunderbolt"},
};

// lets the config name a bit instead of writing the number. Any other value can be given as a
// plain number, which is what a mod with its own item bits needs.
static const struct { const char *name; int bit; } wheelitembits[] =
{
	{"IT_SHOTGUN", IT_SHOTGUN},
	{"IT_SUPER_SHOTGUN", IT_SUPER_SHOTGUN},
	{"IT_NAILGUN", IT_NAILGUN},
	{"IT_SUPER_NAILGUN", IT_SUPER_NAILGUN},
	{"IT_GRENADE_LAUNCHER", IT_GRENADE_LAUNCHER},
	{"IT_ROCKET_LAUNCHER", IT_ROCKET_LAUNCHER},
	{"IT_LIGHTNING", IT_LIGHTNING},
	{"IT_SUPER_LIGHTNING", IT_SUPER_LIGHTNING},
	{"IT_AXE", IT_AXE},
	{"HIT_PROXIMITY_GUN", HIT_PROXIMITY_GUN},
	{"HIT_MJOLNIR", HIT_MJOLNIR},
	{"HIT_LASER_CANNON", HIT_LASER_CANNON},
	{"RIT_AXE", RIT_AXE},
	{"RIT_LAVA_NAILGUN", RIT_LAVA_NAILGUN},
	{"RIT_LAVA_SUPER_NAILGUN", RIT_LAVA_SUPER_NAILGUN},
	{"RIT_MULTI_GRENADE", RIT_MULTI_GRENADE},
	{"RIT_MULTI_ROCKET", RIT_MULTI_ROCKET},
	{"RIT_PLASMA_GUN", RIT_PLASMA_GUN},
};

static wheelweapon_t wheelweapons[MAX_WHEEL_SLOTS];
static int wheelweaponcount;
static char wheelconfigmod[MAX_QPATH];
static qboolean wheelconfigloaded;

typedef struct wheelslot_s
{
	const wheelweapon_t *weapon;
	dp_model_t *model;
	qboolean owned;
	float grow;			// 0 = normal size, 1 = fully highlighted
	vec3_t origin;
}
wheelslot_t;

static wheelslot_t wheelslots[MAX_WHEEL_SLOTS];
static int wheelslotcount;
static float wheelspin;
static vec3_t wheelcursorworld;
static float wheel_savedslowmo = 1.0f;
static qboolean wheel_slowmoactive = false;

static void CL_WeaponWheel_Reload_f(void);

void R_WeaponWheel_Init(void)
{
	Cvar_RegisterVariable(&vr_weaponwheel);
	Cvar_RegisterVariable(&vr_weaponwheel_distance);
	Cvar_RegisterVariable(&vr_weaponwheel_radius);
	Cvar_RegisterVariable(&vr_weaponwheel_modelsize);
	Cvar_RegisterVariable(&vr_weaponwheel_modelpitch);
	Cvar_RegisterVariable(&vr_weaponwheel_modelyaw);
	Cvar_RegisterVariable(&vr_weaponwheel_spin);
	Cvar_RegisterVariable(&vr_weaponwheel_deflection);
	Cvar_RegisterVariable(&vr_weaponwheel_slowmo);
	Cvar_RegisterVariable(&vr_weaponwheel_scan);
	Cmd_AddCommand("weaponwheel_reload", CL_WeaponWheel_Reload_f, "re-read " WEAPONWHEEL_CONFIG " and list the weapons it gives for the current mod");
}

/*
================================================================================

weaponwheel.json

The file is one object. Each key is a mod name as reported by com_modname, which is the -game
directory, or the mission pack directory, or "id1" for the base game. The key "default" covers
every mod that has no entry of its own. The value is the list of weapons in ring order.

	{
	  "default": [
	    { "name": "Axe", "item": "IT_AXE", "impulse": 1, "model": "progs/v_axe.mdl" },
	    ...
	  ]
	}

FS_LoadFile searches the mod directory before id1, so a mod may ship its own copy.

================================================================================
*/

typedef struct jsonparser_s
{
	const char *pos;
	const char *end;
}
jsonparser_t;

static void JSON_SkipWhite(jsonparser_t *parser)
{
	while (parser->pos < parser->end && (unsigned char)*parser->pos <= ' ')
		parser->pos++;
}

// pass out = NULL to step over the string without keeping it
static qboolean JSON_ParseString(jsonparser_t *parser, char *out, size_t outsize)
{
	size_t length = 0;

	JSON_SkipWhite(parser);
	if (parser->pos >= parser->end || *parser->pos != '"')
		return false;
	parser->pos++;

	while (parser->pos < parser->end && *parser->pos != '"')
	{
		char c = *parser->pos++;

		if (c == '\\' && parser->pos < parser->end)
		{
			c = *parser->pos++;
			switch (c)
			{
			case 'n': c = '\n'; break;
			case 't': c = '\t'; break;
			case 'r': c = '\r'; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'u':
				// no use for other alphabets here, so drop the code point
				parser->pos += 4;
				if (parser->pos > parser->end)
					return false;
				c = '?';
				break;
			default: break;
			}
		}

		if (out && length + 1 < outsize)
			out[length++] = c;
	}

	if (parser->pos >= parser->end)
		return false;
	parser->pos++;
	if (out && outsize)
		out[length] = 0;
	return true;
}

static qboolean JSON_ParseNumber(jsonparser_t *parser, double *out)
{
	char buffer[64];
	size_t length = 0;

	JSON_SkipWhite(parser);
	while (parser->pos < parser->end && *parser->pos != 0 && length + 1 < sizeof(buffer)
		&& ((*parser->pos >= '0' && *parser->pos <= '9') || strchr("+-.eExXaAbBcCdDfF", *parser->pos)))
		buffer[length++] = *parser->pos++;

	if (!length)
		return false;
	buffer[length] = 0;
	*out = strtod(buffer, NULL);
	return true;
}

static qboolean JSON_SkipValue(jsonparser_t *parser)
{
	JSON_SkipWhite(parser);
	if (parser->pos >= parser->end)
		return false;

	if (*parser->pos == '"')
		return JSON_ParseString(parser, NULL, 0);

	if (*parser->pos == '{' || *parser->pos == '[')
	{
		char open = *parser->pos;
		char close = (open == '{') ? '}' : ']';
		int depth = 0;

		while (parser->pos < parser->end)
		{
			JSON_SkipWhite(parser);
			if (parser->pos >= parser->end)
				return false;
			// strings are stepped over whole, so a bracket inside one cannot unbalance the count
			if (*parser->pos == '"')
			{
				if (!JSON_ParseString(parser, NULL, 0))
					return false;
				continue;
			}
			if (*parser->pos == open)
				depth++;
			else if (*parser->pos == close)
			{
				depth--;
				parser->pos++;
				if (!depth)
					return true;
				continue;
			}
			parser->pos++;
		}
		return false;
	}

	while (parser->pos < parser->end && *parser->pos != ',' && *parser->pos != '}'
		&& *parser->pos != ']' && (unsigned char)*parser->pos > ' ')
		parser->pos++;
	return true;
}

static int CL_WeaponWheel_ItemBitForName(const char *name)
{
	int i;
	for (i = 0;i < (int)(sizeof(wheelitembits) / sizeof(wheelitembits[0]));i++)
		if (!strcmp(wheelitembits[i].name, name))
			return wheelitembits[i].bit;
	Con_Printf("weapon wheel: unknown item bit \"%s\"\n", name);
	return 0;
}

static qboolean CL_WeaponWheel_ParseWeapon(jsonparser_t *parser, wheelweapon_t *weapon)
{
	memset(weapon, 0, sizeof(*weapon));

	JSON_SkipWhite(parser);
	if (parser->pos >= parser->end || *parser->pos != '{')
		return false;
	parser->pos++;

	for (;;)
	{
		char key[64];

		JSON_SkipWhite(parser);
		if (parser->pos >= parser->end)
			return false;
		if (*parser->pos == '}')
		{
			parser->pos++;
			return true;
		}
		if (*parser->pos == ',')
		{
			parser->pos++;
			continue;
		}

		if (!JSON_ParseString(parser, key, sizeof(key)))
			return false;
		JSON_SkipWhite(parser);
		if (parser->pos >= parser->end || *parser->pos != ':')
			return false;
		parser->pos++;

		if (!strcmp(key, "name"))
		{
			if (!JSON_ParseString(parser, weapon->name, sizeof(weapon->name)))
				return false;
		}
		else if (!strcmp(key, "model"))
		{
			if (!JSON_ParseString(parser, weapon->modelname, sizeof(weapon->modelname)))
				return false;
		}
		else if (!strcmp(key, "impulse"))
		{
			double value;
			if (!JSON_ParseNumber(parser, &value))
				return false;
			weapon->impulse = (int)value;
		}
		else if (!strcmp(key, "item"))
		{
			JSON_SkipWhite(parser);
			if (parser->pos < parser->end && *parser->pos == '"')
			{
				char bitname[64];
				if (!JSON_ParseString(parser, bitname, sizeof(bitname)))
					return false;
				weapon->itembit = CL_WeaponWheel_ItemBitForName(bitname);
			}
			else
			{
				double value;
				if (!JSON_ParseNumber(parser, &value))
					return false;
				weapon->itembit = (int)(unsigned int)value;
			}
		}
		else if (!JSON_SkipValue(parser))
			return false;
	}
}

static qboolean CL_WeaponWheel_ParseWeaponList(jsonparser_t *parser)
{
	int count = 0;

	JSON_SkipWhite(parser);
	// accept both a bare array and an object with a "weapons" array
	if (parser->pos < parser->end && *parser->pos == '{')
	{
		parser->pos++;
		for (;;)
		{
			char key[64];

			JSON_SkipWhite(parser);
			if (parser->pos >= parser->end)
				return false;
			if (*parser->pos == '}')
				return false;
			if (*parser->pos == ',')
			{
				parser->pos++;
				continue;
			}
			if (!JSON_ParseString(parser, key, sizeof(key)))
				return false;
			JSON_SkipWhite(parser);
			if (parser->pos >= parser->end || *parser->pos != ':')
				return false;
			parser->pos++;
			if (!strcmp(key, "weapons"))
				break;
			if (!JSON_SkipValue(parser))
				return false;
		}
		JSON_SkipWhite(parser);
	}

	if (parser->pos >= parser->end || *parser->pos != '[')
		return false;
	parser->pos++;

	for (;;)
	{
		wheelweapon_t weapon;

		JSON_SkipWhite(parser);
		if (parser->pos >= parser->end)
			return false;
		if (*parser->pos == ']')
		{
			parser->pos++;
			wheelweaponcount = count;
			return count > 0;
		}
		if (*parser->pos == ',')
		{
			parser->pos++;
			continue;
		}

		if (!CL_WeaponWheel_ParseWeapon(parser, &weapon))
			return false;
		if (!weapon.itembit || !weapon.impulse)
		{
			Con_Printf("weapon wheel: \"%s\" needs both an item and an impulse, skipped\n", weapon.name);
			continue;
		}
		if (count >= MAX_WHEEL_SLOTS)
		{
			Con_Printf("weapon wheel: more than %i weapons, the rest are ignored\n", MAX_WHEEL_SLOTS);
			continue;
		}
		wheelweapons[count++] = weapon;
	}
}

// walks the top level object looking for one key, and parses its value when it matches
static qboolean CL_WeaponWheel_ParseSection(const char *file, fs_offset_t filesize, const char *wanted)
{
	jsonparser_t parser;

	parser.pos = file;
	parser.end = file + filesize;

	JSON_SkipWhite(&parser);
	if (parser.pos >= parser.end || *parser.pos != '{')
		return false;
	parser.pos++;

	for (;;)
	{
		char key[MAX_QPATH];

		JSON_SkipWhite(&parser);
		if (parser.pos >= parser.end || *parser.pos == '}')
			return false;
		if (*parser.pos == ',')
		{
			parser.pos++;
			continue;
		}

		if (!JSON_ParseString(&parser, key, sizeof(key)))
			return false;
		JSON_SkipWhite(&parser);
		if (parser.pos >= parser.end || *parser.pos != ':')
			return false;
		parser.pos++;

		if (!strcmp(key, wanted))
			return CL_WeaponWheel_ParseWeaponList(&parser);
		if (!JSON_SkipValue(&parser))
			return false;
	}
}

static void CL_WeaponWheel_UseBuiltinWeapons(void)
{
	wheelweaponcount = (int)(sizeof(wheelweapons_builtin) / sizeof(wheelweapons_builtin[0]));
	memcpy(wheelweapons, wheelweapons_builtin, sizeof(wheelweapons_builtin));
}

static void CL_WeaponWheel_LoadConfig(void)
{
	fs_offset_t filesize = 0;
	unsigned char *file;

	strlcpy(wheelconfigmod, com_modname, sizeof(wheelconfigmod));
	wheelconfigloaded = true;
	CL_WeaponWheel_UseBuiltinWeapons();

	file = FS_LoadFile(WEAPONWHEEL_CONFIG, tempmempool, true, &filesize);
	if (!file)
		return;

	if (CL_WeaponWheel_ParseSection((const char *)file, filesize, com_modname))
		Con_DPrintf("weapon wheel: %i weapons from " WEAPONWHEEL_CONFIG " section \"%s\"\n", wheelweaponcount, com_modname);
	else if (CL_WeaponWheel_ParseSection((const char *)file, filesize, "default"))
		Con_DPrintf("weapon wheel: %i weapons from " WEAPONWHEEL_CONFIG " section \"default\"\n", wheelweaponcount);
	else
	{
		Con_Printf("weapon wheel: no usable section for \"%s\" in " WEAPONWHEEL_CONFIG ", using the built in list\n", com_modname);
		CL_WeaponWheel_UseBuiltinWeapons();
	}

	Mem_Free(file);
}

static void CL_WeaponWheel_Reload_f(void)
{
	int i;

	CL_WeaponWheel_LoadConfig();
	Con_Printf("weapon wheel: %i weapons for \"%s\"\n", wheelweaponcount, wheelconfigmod);
	for (i = 0;i < wheelweaponcount;i++)
		Con_Printf("  %2i  item %-10i impulse %-4i %-20s %s\n", i + 1,
			wheelweapons[i].itembit, wheelweapons[i].impulse, wheelweapons[i].name, wheelweapons[i].modelname);
}

// prefer the map's own precache, which is always fully loaded and owned by the map
static dp_model_t *CL_WeaponWheel_FindModel(const char *name)
{
	int i;
	dp_model_t *model;

	for (i = 1;i < MAX_MODELS && cl.model_name[i][0];i++)
		if (!strcmp(cl.model_name[i], name))
			return cl.model_precache[i];

	model = Mod_ForName(name, false, false, NULL);
	return (model && model->loaded) ? model : NULL;
}

qboolean CL_WeaponWheel_CanOpen(void)
{
	qboolean cldead = (cl.stats[STAT_HEALTH] <= 0 && cl.stats[STAT_HEALTH] != -666 && cl.stats[STAT_HEALTH] != -2342);

	if (!vr_weaponwheel.integer)
		return false;
	if (cls.state != ca_connected || cls.signon != SIGNONS)
		return false;
	if (cl.intermission || cls.demoplayback || cldead)
		return false;
	return true;
}

void CL_WeaponWheel_Open(void)
{
	int i;

	if (!wheelconfigloaded || strcmp(wheelconfigmod, com_modname))
		CL_WeaponWheel_LoadConfig();

	wheelslotcount = 0;
	weaponwheel_selection = -1;
	wheelspin = 0.0f;

	for (i = 0;i < wheelweaponcount;i++)
	{
		wheelslot_t *slot = &wheelslots[wheelslotcount++];
		slot->weapon = &wheelweapons[i];
		slot->owned = (cl.stats[STAT_ITEMS] & wheelweapons[i].itembit) != 0;
		slot->model = slot->owned ? CL_WeaponWheel_FindModel(wheelweapons[i].modelname) : NULL;
		slot->grow = 0.0f;
		VectorClear(slot->origin);
	}

	weaponwheel_active = 1;

	if (!wheel_slowmoactive && vr_weaponwheel_slowmo.value < 1.0f && vr_weaponwheel_slowmo.value > 0.0f
		&& cl.maxclients <= 1 && !cls.demoplayback)
	{
		wheel_savedslowmo = slowmo.value;
		wheel_slowmoactive = true;
		Cvar_SetValueQuick(&slowmo, vr_weaponwheel_slowmo.value);
	}
}

void CL_WeaponWheel_Close(void)
{
	weaponwheel_active = 0;
	weaponwheel_selection = -1;
	wheelslotcount = 0;

	if (wheel_slowmoactive)
	{
		wheel_slowmoactive = false;
		Cvar_SetValueQuick(&slowmo, wheel_savedslowmo);
	}
}

void CL_WeaponWheel_Select(void)
{
	int selection = weaponwheel_selection;
	int impulse = 0;
	char vabuf[1024];

	if (selection >= 0 && selection < wheelslotcount && wheelslots[selection].owned
		&& cl.stats[STAT_ACTIVEWEAPON] != wheelslots[selection].weapon->itembit)
		impulse = wheelslots[selection].weapon->impulse;

	CL_WeaponWheel_Close();

	if (impulse)
		Cbuf_AddText(va(vabuf, sizeof(vabuf), "impulse %i\n", impulse));
}

// v_*.mdl models have their origin at the grip, and they differ widely in length. Centre each
// model on its slot and scale it so every weapon reads at the same size on the ring.
static float CL_WeaponWheel_ModelScale(const dp_model_t *model, float targetsize)
{
	float biggest = model->normalmaxs[0] - model->normalmins[0];
	float sy = model->normalmaxs[1] - model->normalmins[1];
	float sz = model->normalmaxs[2] - model->normalmins[2];

	if (sy > biggest)
		biggest = sy;
	if (sz > biggest)
		biggest = sz;
	if (biggest < 0.001f)
		biggest = 0.001f;
	return targetsize / biggest;
}

static void CL_WeaponWheel_PlaceModel(entity_render_t *entrender, const vec3_t target, const vec3_t angles, float scale)
{
	matrix4x4_t rotation;
	vec3_t centre, offset, origin;

	centre[0] = (entrender->model->normalmins[0] + entrender->model->normalmaxs[0]) * 0.5f;
	centre[1] = (entrender->model->normalmins[1] + entrender->model->normalmaxs[1]) * 0.5f;
	centre[2] = (entrender->model->normalmins[2] + entrender->model->normalmaxs[2]) * 0.5f;

	Matrix4x4_CreateFromQuakeEntity(&rotation, 0, 0, 0, angles[0], angles[1], angles[2], scale);
	Matrix4x4_Transform(&rotation, centre, offset);
	VectorSubtract(target, offset, origin);

	Matrix4x4_CreateFromQuakeEntity(&entrender->matrix, origin[0], origin[1], origin[2],
		angles[0], angles[1], angles[2], scale);
}

static float CL_WeaponWheel_AngleDifference(float a, float b)
{
	float difference = a - b;
	while (difference > 180.0f)
		difference -= 360.0f;
	while (difference < -180.0f)
		difference += 360.0f;
	return difference;
}

static entity_render_t *CL_WeaponWheel_NewEntity(dp_model_t *model)
{
	entity_render_t *entrender = CL_NewTempEntity(0);

	if (!entrender)
		return NULL;

	entrender->model = model;
	// no RENDER_LIGHT, so the models render fullbright and stay readable in a dark room.
	// RENDER_NODEPTHTEST keeps the wheel in front of nearby walls.
	entrender->flags = RENDER_NODEPTHTEST;
	entrender->alpha = 1.0f;
	// CL_NewTempEntity zeroes this, and an animated model draws nothing at lerp 0
	entrender->framegroupblend[0].frame = 0;
	entrender->framegroupblend[0].start = cl.time;
	entrender->framegroupblend[0].lerp = 1.0f;
	return entrender;
}

// places the slots in world space, picks the one under the cursor and adds the models to the scene
void CL_WeaponWheel_Relink(void)
{
	int i;
	int previousselection = weaponwheel_selection;
	float worldscale, dist, radius, cursorlength, cursorangle, bestangle;
	float modelsize, framelerp;
	vec3_t forward, right, up, hub, angles;

	// reports what a mod actually sets, so its weaponwheel.json section can be written by hand
	if (vr_weaponwheel_scan.integer)
	{
		static int scanitems = -1;
		static int scanweapon = -1;

		if (cl.stats[STAT_ITEMS] != scanitems || cl.stats[STAT_ACTIVEWEAPON] != scanweapon)
		{
			int modelindex = cl.stats[STAT_WEAPON];

			scanitems = cl.stats[STAT_ITEMS];
			scanweapon = cl.stats[STAT_ACTIVEWEAPON];
			Con_Printf("weaponwheel scan: mod \"%s\"  owned 0x%08x  active item %i (0x%08x)  model \"%s\"\n",
				com_modname, (unsigned int)scanitems, scanweapon, (unsigned int)scanweapon,
				(modelindex > 0 && modelindex < MAX_MODELS) ? cl.model_name[modelindex] : "");
		}
	}

	if (!weaponwheel_active || !wheelslotcount)
		return;

	worldscale = bound(1.0f, vr_worldscale.value, 60.0f);
	dist = vr_weaponwheel_distance.value * worldscale;
	radius = vr_weaponwheel_radius.value * worldscale;
	modelsize = vr_weaponwheel_modelsize.value * worldscale;

	AngleVectors(weaponwheel_angles, forward, right, up);
	VectorMA(gunorg, dist, forward, hub);

	VectorMA(hub, radius * weaponwheel_cursor[0], right, wheelcursorworld);
	VectorMA(wheelcursorworld, radius * weaponwheel_cursor[1], up, wheelcursorworld);

	// walk the ring by rolling the wheel plane, which keeps every slot on the disc
	for (i = 0;i < wheelslotcount;i++)
	{
		vec3_t slotangles, slotup;

		slotangles[PITCH] = weaponwheel_angles[PITCH];
		slotangles[YAW] = weaponwheel_angles[YAW];
		slotangles[ROLL] = (360.0f / wheelslotcount) * i;
		AngleVectors(slotangles, NULL, NULL, slotup);
		VectorMA(hub, radius, slotup, wheelslots[i].origin);
	}

	// pick the owned slot closest in angle to the cursor. Measuring the slot angle from its own
	// world position avoids any assumption about which way the roll turns.
	cursorlength = sqrt(weaponwheel_cursor[0] * weaponwheel_cursor[0] + weaponwheel_cursor[1] * weaponwheel_cursor[1]);
	weaponwheel_selection = -1;
	bestangle = 360.0f;
	if (cursorlength >= 0.35f)
	{
		cursorangle = RAD2DEG(atan2(weaponwheel_cursor[0], weaponwheel_cursor[1]));
		for (i = 0;i < wheelslotcount;i++)
		{
			vec3_t slotdir;
			float difference;

			if (!wheelslots[i].owned)
				continue;

			VectorSubtract(wheelslots[i].origin, hub, slotdir);
			difference = fabs(CL_WeaponWheel_AngleDifference(cursorangle,
				RAD2DEG(atan2(DotProduct(slotdir, right), DotProduct(slotdir, up)))));
			if (difference < bestangle)
			{
				bestangle = difference;
				weaponwheel_selection = i;
			}
		}
	}

	if (weaponwheel_selection != previousselection && weaponwheel_selection >= 0)
		TBXR_Vibrate(50, cl_righthanded.integer ? 2 : 1, 0.6f);

	// the wheel runs while the game is slowed down, so drive both animations off real time
	framelerp = bound(0.0f, (float)(cl.realframetime * 12.0), 1.0f);
	wheelspin += (float)(cl.realframetime * vr_weaponwheel_spin.value);
	while (wheelspin >= 360.0f)
		wheelspin -= 360.0f;
	while (wheelspin < 0.0f)
		wheelspin += 360.0f;

	VectorClear(angles);
	angles[PITCH] = vr_weaponwheel_modelpitch.value;
	angles[YAW] = weaponwheel_angles[YAW] + vr_weaponwheel_modelyaw.value + wheelspin;

	for (i = 0;i < wheelslotcount;i++)
	{
		entity_render_t *entrender;
		float shade, size;

		if (!wheelslots[i].owned || !wheelslots[i].model)
			continue;

		wheelslots[i].grow += ((i == weaponwheel_selection ? 1.0f : 0.0f) - wheelslots[i].grow) * framelerp;

		entrender = CL_WeaponWheel_NewEntity(wheelslots[i].model);
		if (!entrender)
			return;

		size = modelsize * (1.0f + 0.6f * wheelslots[i].grow);
		shade = 0.55f + 0.95f * wheelslots[i].grow;
		VectorSet(entrender->colormod, shade, shade, shade * 0.9f);
		CL_WeaponWheel_PlaceModel(entrender, wheelslots[i].origin, angles,
			CL_WeaponWheel_ModelScale(wheelslots[i].model, size));
		CL_UpdateRenderEntity(entrender);
	}

	// the weapon currently held sits at the hub, so the player can see what they are swapping from
	for (i = 0;i < wheelslotcount;i++)
	{
		entity_render_t *entrender;

		if (cl.stats[STAT_ACTIVEWEAPON] != wheelslots[i].weapon->itembit || !wheelslots[i].model)
			continue;

		entrender = CL_WeaponWheel_NewEntity(wheelslots[i].model);
		if (!entrender)
			return;

		VectorSet(entrender->colormod, 0.45f, 0.45f, 0.5f);
		CL_WeaponWheel_PlaceModel(entrender, hub, angles,
			CL_WeaponWheel_ModelScale(wheelslots[i].model, modelsize * 0.7f));
		CL_UpdateRenderEntity(entrender);
		break;
	}
}

void R_DrawWeaponWheel(void)
{
	float thickness, crosssize;
	vec3_t forward, right, up, a, b;

	if (!weaponwheel_active || !wheelslotcount)
		return;

	thickness = 0.008f * bound(1.0f, vr_worldscale.value, 60.0f);
	crosssize = thickness * 4.0f;
	AngleVectors(weaponwheel_angles, forward, right, up);

	GL_DepthTest(false);
	GL_CullFace(r_refdef.view.cullface_front);

	// pointer beam from the hand to the cursor
	R_DrawBLineMesh(gunorg, wheelcursorworld, thickness, 0.2f, 0.6f, 1.0f, 1.0f);

	// small cross so the cursor stays visible against a busy background
	VectorMA(wheelcursorworld, -crosssize, right, a);
	VectorMA(wheelcursorworld, crosssize, right, b);
	R_DrawBLineMesh(a, b, thickness, 1.0f, 1.0f, 1.0f, 1.0f);
	VectorMA(wheelcursorworld, -crosssize, up, a);
	VectorMA(wheelcursorworld, crosssize, up, b);
	R_DrawBLineMesh(a, b, thickness, 1.0f, 1.0f, 1.0f, 1.0f);
}

// the name of the highlighted weapon, drawn flat with the same per eye parallax the crosshair uses
void R_WeaponWheel_DrawText(void)
{
	const char *text;
	float width, x, y;
	float hudoffsetx, hudoffsety;
	int stereooffset;

	if (!weaponwheel_active || weaponwheel_selection < 0 || weaponwheel_selection >= wheelslotcount)
		return;

	text = wheelslots[weaponwheel_selection].weapon->name;
	width = DrawQ_TextWidth(text, 0, 16, 16, true, FONT_CENTERPRINT);
	stereooffset = vr_worldscale.value > 200.0f ? 12 : 5;
	GetHUDOffset(&hudoffsetx, &hudoffsety);
	x = (vid_conwidth.integer - width) * 0.5f + (r_stereo_side ? -stereooffset : stereooffset) + hudoffsetx;
	y = vid_conheight.integer * 0.68f + hudoffsety;

	DrawQ_String(x, y, text, 0, 16, 16, 1, 0.9f, 0.5f, 1, 0, NULL, true, FONT_CENTERPRINT);
}
