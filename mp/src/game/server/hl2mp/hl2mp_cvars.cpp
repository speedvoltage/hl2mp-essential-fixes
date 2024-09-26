//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2mp_cvars.h"

ConVar mp_noweapons(
	"mp_noweapons",
	"0",
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, game will not give player default weapons and ammo");

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