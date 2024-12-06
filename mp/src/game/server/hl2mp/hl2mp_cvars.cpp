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
							"If non-zero, game will restart once each player gives the ready signal" );

// Ready signal
ConVar mp_ready_signal(
							"mp_ready_signal",
							"ready",
							FCVAR_GAMEDLL,
							"Text that each player must speak for the match to begin" );

ConVar sv_soundfix( "sv_soundfix", 
	"1", 
	FCVAR_GAMEDLL, 
	"If non-zero, apply the sound fixes to all sounds" );