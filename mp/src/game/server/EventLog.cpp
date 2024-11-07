//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "EventLog.h"
#include "team.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_show_client_disconnect_msg;

CEventLog::CEventLog()
{
}

CEventLog::~CEventLog()
{
}


void CEventLog::FireGameEvent( IGameEvent *event )
{
	PrintEvent ( event );
}

bool CEventLog::PrintEvent( IGameEvent *event )
{
	const char * name = event->GetName();

	if ( Q_strncmp(name, "server_", strlen("server_")) == 0 )
	{
		return true; // we don't care about server events (engine does)
	}
	else if ( Q_strncmp(name, "player_", strlen("player_")) == 0 )
	{
		return PrintPlayerEvent( event );
	}
	else if ( Q_strncmp(name, "team_", strlen("team_")) == 0 )
	{
		return PrintTeamEvent( event );
	}
	else if ( Q_strncmp(name, "game_", strlen("game_")) == 0 )
	{
		return PrintGameEvent( event );
	}
	else
	{
		return PrintOtherEvent( event ); // bomb_, round_, et al
	}
}

extern ConVar net_maskclientipaddress;
bool CEventLog::PrintGameEvent( IGameEvent *event )
{
//	const char * name = event->GetName() + Q_strlen("game_"); // remove prefix

	return false;
}

static void RemoveDisallowedCharacters(char* str, const char** disallowedStrings, int disallowedCount, int strLength)
{
	for (int i = 0; i < disallowedCount; i++)
	{
		const char* disallowed = disallowedStrings[i];
		char* pos;

		while ((pos = strstr(str, disallowed)) != nullptr)
		{
			int len = strlen(disallowed);
			memmove(pos, pos + len, strLength - (pos - str) - len + 1); 
		}
	}
}

bool CEventLog::PrintPlayerEvent( IGameEvent *event )
{
	const char * eventName = event->GetName();
	const int userid = event->GetInt( "userid" );

	if ( !Q_strncmp( eventName, "player_connect", Q_strlen("player_connect") ) ) // player connect is before the CBasePlayer pointer is setup
	{
		const char* name = event->GetString("name");
		const char *address = event->GetString( "address" );
		const char *networkid = event->GetString("networkid" );
		if ( !net_maskclientipaddress.GetBool() )
			UTIL_LogPrintf( "\"%s<%i><%s><>\" connected, address \"%s\"\n", name, userid, networkid, address );
		else
			UTIL_LogPrintf( "\"%s<%i><%s><>\" connected\n", name, userid, networkid );

		if (Q_strcmp(eventName, "player_connect_client") == 0)
		{
			event->SetString("name", "NULLNAME");
		}
		return true;
	}
	else if ( !Q_strncmp( eventName, "player_disconnect", Q_strlen("player_disconnect")  ) )
	{
		const char *reason = event->GetString( "reason" );
		const char *name = event->GetString("name" );
		const char *networkid = event->GetString("networkid" );
		CTeam *team = NULL;
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );

		if ( pPlayer )
		{
			team = pPlayer->GetTeam();
		}

		char sanitizedReason[256];
		V_strncpy(sanitizedReason, reason, sizeof(sanitizedReason));

		const char *disallowedStrings[] = { "\n", "\t", "\r", "\x01", "\x02", "\x03", "\x04", "\x05", "\x06", "\x07", "\x08" };
		int disallowedCount = sizeof(disallowedStrings) / sizeof(disallowedStrings[0]);

		RemoveDisallowedCharacters(sanitizedReason, disallowedStrings, disallowedCount, sizeof(sanitizedReason));

		event->SetString("reason", sanitizedReason);

		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected (reason \"%s\")\n", name, userid, networkid, team ? team->GetName() : "", reason );
		
		if (sv_show_client_disconnect_msg.GetBool())
			UTIL_PrintToAllClients(CHAT_DEFAULT "%s1 " CHAT_CONTEXT "has disconnected " CHAT_SPEC "(%s2)", name, reason);
		
		if (Q_strcmp(eventName, "player_disconnect") == 0)
		{
			event->SetString("name", "NULLNAME");
		}
		return true;
	}

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if ( !pPlayer)
	{
		DevMsg( "CEventLog::PrintPlayerEvent: Failed to find player (userid: %i, event: %s)\n", userid, eventName );
		return false;
	}

	if ( !Q_strncmp( eventName, "player_team", Q_strlen("player_team") ) )
	{
		const bool bDisconnecting = event->GetBool( "disconnect" );

		if ( !bDisconnecting )
		{
			const int newTeam = event->GetInt( "team" );
			const int oldTeam = event->GetInt( "oldteam" );
			CTeam *team = GetGlobalTeam( newTeam );
			CTeam *oldteam = GetGlobalTeam( oldTeam );
			
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"%s\"\n", 
			pPlayer->GetPlayerName(),
			pPlayer->GetUserID(),
			pPlayer->GetNetworkIDString(),
			oldteam->GetName(),
			team->GetName() );
		}

		return true;
	}
	else if ( !Q_strncmp( eventName, "player_death", Q_strlen("player_death") ) )
	{
		const int attackerid = event->GetInt("attacker" );

#ifdef HL2MP
		const char *weapon = event->GetString( "weapon" );
#endif
		
		CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
		CTeam *team = pPlayer->GetTeam();
		CTeam *attackerTeam = NULL;
		
		if ( pAttacker )
		{
			attackerTeam = pAttacker->GetTeam();
		}
		if ( pPlayer == pAttacker && pPlayer )  
		{  

#ifdef HL2MP
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : "",
							weapon
							);
#else
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : "",
							pAttacker->GetClassname()
							);
#endif
		}
		else if ( pAttacker )
		{
			CTeam *attackerTeam = pAttacker->GetTeam();

#ifdef HL2MP
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
							pAttacker->GetPlayerName(),
							attackerid,
							pAttacker->GetNetworkIDString(),
							attackerTeam ? attackerTeam->GetName() : "",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : "",
							weapon
							);
#else
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\"\n",  
							pAttacker->GetPlayerName(),
							attackerid,
							pAttacker->GetNetworkIDString(),
							attackerTeam ? attackerTeam->GetName() : "",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : ""
							);								
#endif
		}
		else
		{  
			// killed by the world
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"world\"\n",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : ""
							);
		}
		return true;
	}
	else if ( !Q_strncmp( eventName, "player_activate", Q_strlen("player_activate") ) )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><>\" entered the game\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString()
							);

		return true;
	}
	else if ( !Q_strncmp( eventName, "player_changename", Q_strlen("player_changename") ) )
	{
		const char *newName = event->GetString( "newname" );
		const char *oldName = event->GetString( "oldname" );
		CTeam *team = pPlayer->GetTeam();
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
					oldName,
					userid,
					pPlayer->GetNetworkIDString(),
					team ? team->GetName() : "",
					newName
					);
		return true;
	}
			
	else if ( !Q_strncmp( eventName, "player_hurt", Q_strlen( "player_hurt" ) ) )
	{
		const int attackerid = event->GetInt("attacker");
		const int dmgType = event->GetInt("dmgtype");

		CBasePlayer* pAttacker = UTIL_PlayerByUserId(attackerid);

		if ( pAttacker &&
			pAttacker != pPlayer &&
			pAttacker->AreHitSoundsEnabled() &&
			( dmgType & ( DMG_BLAST | DMG_CLUB | DMG_CRUSH ) ) )
		{
			CRecipientFilter filter;
			filter.AddRecipient( pAttacker );
			filter.MakeReliable();

			EmitSound_t params;
			params.m_pSoundName = "server_sounds_hitbody";
			params.m_flSoundTime = 0;
			params.m_pOrigin = &pPlayer->GetAbsOrigin();

			pPlayer->EmitSound( filter, pAttacker->entindex(), params );
		}

		return true;
	}
// ignored events
//player_hurt
	return false;
}

bool CEventLog::PrintTeamEvent( IGameEvent *event )
{
//	const char * name = event->GetName() + Q_strlen("team_"); // remove prefix

	return false;
}

bool CEventLog::PrintOtherEvent( IGameEvent *event )
{
	return false;
}


bool CEventLog::Init()
{
	ListenForGameEvent( "player_changename" );
	ListenForGameEvent( "player_activate" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_hurt" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_connect_client" );

	return true;
}

void CEventLog::Shutdown()
{
	StopListeningForAllEvents();
}