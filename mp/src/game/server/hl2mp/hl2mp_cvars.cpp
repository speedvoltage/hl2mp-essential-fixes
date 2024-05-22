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
	"mp_noweapons",
	"0",
	FCVAR_GAMEDLL,
	"If non-zero, game will not give player default weapons and ammo");

// Enable suit notifications in multiplayer
ConVar mp_suitvoice(
	"mp_suitvoice",
	"0",
	FCVAR_GAMEDLL,
	"If non-zero, game will enable suit notifications");

ConVar sv_game_description(
	"sv_game_description",
	"Classic Deathmatch",
	FCVAR_GAMEDLL,
	"Sets the game description");

ConVar mp_ear_ringing(
	"mp_ear_ringing",
	"0",
	FCVAR_GAMEDLL | FCVAR_REPLICATED,
	"If non-zero, produce ringing sound caused by explosion/blast damage");