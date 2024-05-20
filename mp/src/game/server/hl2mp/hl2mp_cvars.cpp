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

ConVar sv_switch_messages(
	"sv_switch_messages", 
	"0", 
	FCVAR_GAMEDLL | FCVAR_NOTIFY,
	"If non-zero, game will notify the player of wait for switch messages.");