//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== tf_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

#include "cbase.h"
#include "hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "team.h"
#include "viewport_panel_names.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

ConVar sv_motd_unload_on_dismissal( "sv_motd_unload_on_dismissal", "0", 0, "If enabled, the MOTD contents will be unloaded when the player closes the MOTD." );
ConVar sv_show_motd_on_connect("sv_show_motd_on_connect", "0", 0, "If enabled, shows the MOTD to the player when fully put into the server.");
ConVar sv_show_client_put_in_server_msg("sv_show_client_put_in_server_msg", "1", 0, "Prints to all client that a connecting player is fully put in the server.");
ConVar sv_join_spec_on_connect("sv_join_spec_on_connect", "0", 0, "If non-zero, put connecting players to team spectators on fully joined");

extern ConVar sv_timeleft_color_override;
extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

bool g_bWasGamePausedOnJoin = false;

// NOTE: Should we make it so that NOSTEAM servers are not allowed to use those binaries?

#ifndef NO_STEAM
void CHL2MP_Player::AuthenticationCheckThink()
{
	if (!IsConnected())
		return;

	const char* steamID3 = engine->GetPlayerNetworkIDString(this->edict());

	if (!steamID3 || (V_stristr(steamID3, "PENDING") != nullptr))
	{
		engine->ServerCommand(UTIL_VarArgs("kickid %d Authentication failed. Ensure you are logged into Steam and try reconnecting\n", this->GetUserID()));
		return;
	}

	if (GetUserID() < 1)
	{
		engine->ServerCommand(UTIL_VarArgs("kickid %d Invalid UserID\n", GetUserID()));
		return;
	}

	SetContextThink(NULL, TICK_NEVER_THINK, "AuthenticationCheckThink");
}
#endif

void FinishClientPutInServer( CHL2MP_Player *pPlayer )
{
#ifndef NO_STEAM
	pPlayer->SetContextThink(
		static_cast<void (CBaseEntity::*)()>(&CHL2MP_Player::AuthenticationCheckThink),
		gpGlobals->curtime + 60.0f,
		"AuthenticationCheckThink"
	);

#endif

	pPlayer->InitialSpawn();

	if (sv_join_spec_on_connect.GetBool())
		pPlayer->ChangeTeam(TEAM_SPECTATOR);

	pPlayer->Spawn();

	if (pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
	{
		pPlayer->SetObserverMode(OBS_MODE_ROAMING);
	}

	char sName[128];
	Q_strncpy( sName, pPlayer->GetPlayerName(), sizeof( sName ) );
	
	// First parse the name and remove any %'s
	for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
	{
		// Replace it with a space
		if ( *pApersand == '%' )
				*pApersand = ' ';
	}

	// notify other clients of player joining the game
	if (sv_show_client_put_in_server_msg.GetBool())
		UTIL_PrintToAllClients(CHAT_DEFAULT "%s1 " CHAT_CONTEXT "is connected.", sName[0] != 0 ? sName : "<unconnected>");

	if (sv_show_motd_on_connect.GetBool())
	{
		const ConVar* hostname = cvar->FindVar("hostname");
		const char* title = (hostname) ? hostname->GetString() : "MESSAGE OF THE DAY";

		KeyValues* data = new KeyValues("data");
		data->SetString("title", title);		// info panel title
		data->SetString("type", "1");			// show userdata from stringtable entry
		data->SetString("msg", "motd");		// use this stringtable entry
		data->SetBool("unload", sv_motd_unload_on_dismissal.GetBool());

		pPlayer->ShowViewPortPanel(PANEL_INFO, true, data);

		data->deleteThis();
	}

	// If on a custom game mode that puts players in team spectator on connect, strip suit and weapons too
	if (pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
	{
		pPlayer->RemoveAllItems(true);
	}

	if (sv_join_spec_on_connect.GetBool())
		pPlayer->ChangeTeam(TEAM_SPECTATOR);
}

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CHL2MP_Player *pPlayer = CHL2MP_Player::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CHL2MP_Player *pPlayer = ToHL2MPPlayer( CBaseEntity::Instance( pEdict ) );
	FinishClientPutInServer( pPlayer );
}

/*
===============
Purpose: Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char* GetGameDescription()
{
	if (g_pGameRules) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life 2 Deathmatch";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	CBaseEntity::PrecacheModel("models/player.mdl");
	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );
	CBaseEntity::PrecacheModel ("models/weapons/v_hands.mdl");

	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );

	CBaseEntity::PrecacheScriptSound( "FX_AntlionImpact.ShellImpact" );
	CBaseEntity::PrecacheScriptSound( "Missile.ShotDown" );
	CBaseEntity::PrecacheScriptSound( "Bullets.DefaultNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.GunshipNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.StriderNearmiss" );
	
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepHigh" );
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepLow" );
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( pEdict );

	if ( pPlayer )
	{
		if ( gpGlobals->curtime > pPlayer->GetDeathTime() + DEATH_ANIMATION_TIME )
		{		
			// respawn player
			pPlayer->Spawn();
		}
		else
		{
			pPlayer->SetNextThink( gpGlobals->curtime + 0.1f );
		}
	}
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);

#ifdef DEBUG
	extern void Bot_RunAll();
	Bot_RunAll();
#endif
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	// vanilla deathmatch
	CreateGameRulesObject( "CHL2MPRules" );

	if (GameRules()->IsTeamplay())
		sv_timeleft_color_override.SetValue(1);
}

