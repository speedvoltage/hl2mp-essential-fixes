//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2mp_cvars.h"

// Ready restart
ConVar mp_readyrestart(
	"mp_readyrestart",
	"0",
	FCVAR_GAMEDLL,
	"If non-zero, game will restart once each player gives the ready signal");

// Ready signal
ConVar mp_ready_signal(
	"mp_ready_signal",
	"ready",
	FCVAR_GAMEDLL,
	"Text that each player must speak for the match to begin");

ConVar mp_noweapons(
	"mp_skipdefaults",
	"0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, game will not give player default weapons and ammo");

ConVar mp_spawnweapons(
	"mp_spawnweapons",
	"0",
	FCVAR_GAMEDLL,
	"If non-zero, spawn player with weapons set in weapon_spawns.txt");

// Enable suit notifications in multiplayer
ConVar mp_suitvoice(
	"mp_suitvoice",
	"0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, game will enable suit notifications");

ConVar mp_ear_ringing(
	"mp_ear_ringing",
	"0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, produce ringing sound caused by explosion/blast damage");

ConVar sv_custom_sounds(
	"sv_custom_sounds",
	"1",
	FCVAR_GAMEDLL,
	"If non-zero, downloads the custom sounds in the server_sounds folder");

ConVar sv_lockteams(
	"sv_lockteams", 
	"0", FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, locks players to their current teams. ONLY USE THIS IN MATCHES!");

ConVar sv_teamsmenu(
	"sv_teamsmenu", 
	"1", FCVAR_GAMEDLL,
	"If non-zero, players can change teams with a menu.");

ConVar sv_showhelpmessages(
	"sv_showhelpmessages",
	"1",
	FCVAR_GAMEDLL);

ConVar mp_restartgame_notimelimitreset(
	"mp_restartgame_notimelimitreset",
	"0",
	FCVAR_GAMEDLL);

ConVar mp_spawnprotection(
	"mp_spawnprotection",
	"0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY);

ConVar mp_spawnprotection_time("mp_spawnprotection_time", 
	"5",
	FCVAR_GAMEDLL | FCVAR_NOTIFY, 
	"How long should the spawn protection last if not moving", 
	true, 3.0, true, 60.0);

ConVar mp_afk("mp_afk", 
	"0", 
	FCVAR_GAMEDLL,
	"Enable or disable the AFK system.");

ConVar mp_afk_time(
	"mp_afk_time", 
	"30", 
	FCVAR_GAMEDLL,
	"Time in seconds after which AFK players will be kicked.",
	true, 30.0f, false, 60.0f);

ConVar mp_afk_warnings(
	"mp_afk_warnings", 
	"1", FCVAR_GAMEDLL,
	"Warn players if they are AFK.");

ConVar sv_teamkill_kick(
	"sv_teamkill_kick", 
	"1", 
	FCVAR_GAMEDLL,
	"Enable or disable kicking players who team kill too much.");

ConVar sv_teamkill_kick_threshold(
	"sv_teamkill_kick_threshold", 
	"5", 
	FCVAR_GAMEDLL,
	"Number of team kills before a player is kicked.");

ConVar sv_teamkill_kick_warning(
	"sv_teamkill_kick_warning", 
	"1", 
	FCVAR_GAMEDLL,
	"Warn players on each team kill about the consequences.");

ConVar sv_domination_messages(
	"sv_domination_messages",
	"1",
	FCVAR_GAMEDLL,
	"Show domination messages to players.");

ConVar sv_infinite_flashlight(
	"sv_infinite_flashlight",
	"1",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, uses infinite aux power for flashlight, else use HL2 mechanics"
);

ConVar sv_propflying(
	"sv_propflying",
	"1",
	FCVAR_NOTIFY,
	"If non-zero, restores the pre-OB prop flying bug");
