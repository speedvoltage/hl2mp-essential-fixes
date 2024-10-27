#include "cbase.h"
#include "hl2mp_serveradmin.h"
#include "filesystem.h"
#include <KeyValues.h>
#include "hl2mp_player.h"
#include "convar.h"
#include "tier0/icommandline.h"
#include <time.h>

// always comes last
#include "tier0/memdbgon.h"

// 10/17/24
#define SA_VERSION	"1.0.0.9"
#define SA_POWERED	"Server Binaries"
CHL2MP_Admin* g_pHL2MPAdmin = NULL;
bool g_bAdminSystem = false;
// global list of admins
CUtlVector<CHL2MP_Admin*> g_AdminList;
FileHandle_t g_AdminLogFile = FILESYSTEM_INVALID_HANDLE;

ConVar sv_showadminpermissions( "sv_showadminpermissions", "1", 0, "If non-zero, a non-root admin will only see the commands they have access to" );

//-----------------------------------------------------------------------------
// Purpose: Constructor/destructor
//-----------------------------------------------------------------------------
CHL2MP_Admin::CHL2MP_Admin( const char* steamID, const char* permissions )
{
	m_steamID = V_strdup( steamID );          // make a copy of the steamID string
	m_permissions = V_strdup( permissions );  // make a copy of the permissions string

	Assert( !g_pHL2MPAdmin );
	g_pHL2MPAdmin = this;
}

CHL2MP_Admin::~CHL2MP_Admin()
{
	if ( m_steamID )
	{
		free( ( void* ) m_steamID );   // free the copied steamID string
	}

	if ( m_permissions )
	{
		free( ( void* ) m_permissions );  // free the copied permissions string
	}

	g_pHL2MPAdmin = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Admin permissions
//-----------------------------------------------------------------------------
bool CHL2MP_Admin::HasPermission( char flag ) const
{
	if ( !m_permissions )
		return false;

	DevMsg( "Checking permission flag %c against permissions %s\n", flag, m_permissions );

	bool hasPermission = ( strchr( m_permissions, flag ) != nullptr ) || ( strchr( m_permissions, ADMIN_ROOT ) != nullptr );

	// quick dev perms check
	if ( hasPermission )
		DevMsg( "Admin has the required permission.\n" );
	else
		DevMsg( "Admin does NOT have the required permission.\n" );

	return hasPermission;
}


//-----------------------------------------------------------------------------
// Purpose: Add a new admin with SteamID and permissions to the global admin list
//-----------------------------------------------------------------------------
void CHL2MP_Admin::AddAdmin( const char* steamID, const char* permissions )
{
	// check if admin already exists
	CHL2MP_Admin* existingAdmin = GetAdmin( steamID );
	if ( existingAdmin )
	{
		Msg( "Admin with SteamID %s already exists.\n", steamID );
		return;
	}

	// steamID needs to be valid
	if ( steamID == nullptr || permissions == nullptr )
	{
		Msg( "Invalid admin data: SteamID or permissions are null.\n" );
		return;
	}

	// if the steamID doesn't exist, add it
	CHL2MP_Admin* pNewAdmin = new CHL2MP_Admin( steamID, permissions );
	g_AdminList.AddToTail( pNewAdmin );

	Msg( "Added admin with SteamID %s and permissions %s.\n", steamID, permissions );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHL2MP_Admin* CHL2MP_Admin::GetAdmin( const char* steamID )
{
	for ( int i = 0; i < g_AdminList.Count(); i++ )
	{
		DevMsg( "Comparing against: %s\n", g_AdminList[ i ]->GetSteamID() );
		if ( Q_stricmp( g_AdminList[ i ]->GetSteamID(), steamID ) == 0 )
		{
			return g_AdminList[ i ];
		}
	}
	return nullptr;  // No admin found
}

//-----------------------------------------------------------------------------
// Purpose: Check if a player's SteamID has admin permissions
//			Different from GetAdmin() just above since we do not
//			directly get the SteamID of an active player on the
//			the server using engine->GetPlayerNetworkIDString( edict )
//-----------------------------------------------------------------------------
static bool IsSteamIDAdmin( const char* steamID )
{
	KeyValues* kv = new KeyValues( "Admins" );

	if ( !kv->LoadFromFile( filesystem, "cfg/admin/admins.txt", "MOD" ) )
	{
		Msg( "Failed to open cfg/admin/admins.txt for reading.\n" );
		kv->deleteThis();
		return false;
	}

	// checking all the admin flags
	const char* adminFlags = kv->GetString( steamID, nullptr );
	if ( adminFlags && *adminFlags != '\0' )
	{
		if ( Q_stristr( adminFlags, "b" ) || Q_stristr( adminFlags, "c" ) || Q_stristr( adminFlags, "d" ) ||
			Q_stristr( adminFlags, "e" ) || Q_stristr( adminFlags, "f" ) || Q_stristr( adminFlags, "g" ) ||
			Q_stristr( adminFlags, "h" ) || Q_stristr( adminFlags, "i" ) || Q_stristr( adminFlags, "j" ) ||
			Q_stristr( adminFlags, "k" ) || Q_stristr( adminFlags, "l" ) || Q_stristr( adminFlags, "m" ) ||
			Q_stristr( adminFlags, "n" ) || Q_stristr( adminFlags, "z" ) )
		{
			kv->deleteThis();
			return true;
		}
	}

	kv->deleteThis();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check if a player has admin permissions
//-----------------------------------------------------------------------------
bool CHL2MP_Admin::IsPlayerAdmin( CBasePlayer* pPlayer, const char* requiredFlags )
{
	if ( !pPlayer )
		return false;

	const char* steamID = engine->GetPlayerNetworkIDString( pPlayer->edict() );

	CHL2MP_Admin* pAdmin = GetAdmin( steamID );

	if ( pAdmin )
	{
		if ( pAdmin->HasPermission( ADMIN_ROOT ) )  // root is z, all permissions
		{
			DevMsg( "Admin has root ('z') flag, granting all permissions.\n" );
			return true;
		}

		// else does this player have at least one flag?
		for ( int i = 0; requiredFlags[ i ] != '\0'; i++ )
		{
			if ( pAdmin->HasPermission( requiredFlags[ i ] ) )
			{
				// they do
				return true;
			}
		}
	}

	// or they don't
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Clear all admins (e.g., when the server resets or map changes)
//-----------------------------------------------------------------------------
void CHL2MP_Admin::ClearAllAdmins()
{
	g_AdminList.PurgeAndDeleteElements();
	DevMsg( "All admins have been cleared from the server.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Admin say
//-----------------------------------------------------------------------------
static void AdminSay( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	// only by a player or server console!!!
	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	// check permission
	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "j" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	// ensure there's a message to send
	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa say <message>\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa say <message>\n" );
		}
		return;
	}

	// get the message text
	CUtlString messageText;
	for ( int i = 2; i < args.ArgC(); ++i )
	{
		messageText.Append( args[ i ] );
		if ( i < args.ArgC() - 1 )
		{
			messageText.Append( " " );
		}
	}

	// format and print the message
	if ( isServerConsole )
	{
		UTIL_PrintToAllClients( UTIL_VarArgs( "\x04(ADMIN) Console: \x01%s\n", messageText.Get() ) );
	}
	else
	{
		UTIL_PrintToAllClients( UTIL_VarArgs( "\x04(ADMIN) %s: \x01%s\n", pPlayer->GetPlayerName(), messageText.Get() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Admin center say
//-----------------------------------------------------------------------------
static void AdminCSay( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	// only by a player or server console!!!
	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	// check permission
	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "j" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	// ensure there's a message to send
	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa csay <message>\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa csay <message>\n" );
		}
		return;
	}

	// get the message text
	CUtlString messageText;
	for ( int i = 2; i < args.ArgC(); ++i )
	{
		messageText.Append( args[ i ] );
		if ( i < args.ArgC() - 1 )
		{
			messageText.Append( " " );
		}
	}

	// format and print the message
	if ( isServerConsole )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, UTIL_VarArgs( "CONSOLE: %s\n", messageText.Get() ) );
	}
	else
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, UTIL_VarArgs( "%s: %s\n", pPlayer->GetPlayerName(), messageText.Get() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Admin chat (only other admins can see those messages)
//-----------------------------------------------------------------------------
static void AdminChat( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "j" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa chat <message>\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa chat <message>\n" );
		}
		return;
	}

	CUtlString messageText;
	for ( int i = 2; i < args.ArgC(); ++i )
	{
		messageText.Append( args[ i ] );
		if ( i < args.ArgC() - 1 )
		{
			messageText.Append( " " );
		}
	}

	CUtlString formattedMessage;
	if ( isServerConsole )
	{
		formattedMessage = UTIL_VarArgs( "\x04(Admin Chat) Console: \x01%s\n", messageText.Get() );
	}
	else
	{
		formattedMessage = UTIL_VarArgs( "\x04(Admin Chat) %s: \x01%s\n", pPlayer->GetPlayerName(), messageText.Get() );
	}

	// send to admins only
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
		// "b" is minimum admin flag for visibility, which is what any admin should have
		if ( pLoopPlayer && CHL2MP_Admin::IsPlayerAdmin( pLoopPlayer, "b" ) )
		{
			UTIL_PrintToClient( pLoopPlayer, formattedMessage.Get() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: private messages
//-----------------------------------------------------------------------------
static void AdminPSay( const CCommand& args )
{
	CBasePlayer* pSender = UTIL_GetCommandClient();
	bool isServerConsole = !pSender && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pSender && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pSender && !CHL2MP_Admin::IsPlayerAdmin( pSender, "j" ) )
	{
		UTIL_PrintToClient( pSender, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 4 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa psay <name|#userID> <message>\n" );
		}
		else
		{
			UTIL_PrintToClient( pSender, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa psay <name|#userID> <message>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	CBasePlayer* pTarget = nullptr;

	if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				UTIL_PrintToClient( pSender, CHAT_RED "No player found with that UserID.\n" );
				return;
			}
		}
		else
		{
			UTIL_PrintToClient( pSender, CHAT_RED "Invalid UserID provided.\n" );
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			UTIL_PrintToClient( pSender, CHAT_RED "No players found matching that name.\n" );
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			UTIL_PrintToClient( pSender, CHAT_ADMIN "Multiple players match that partial name:\n" );
			for ( int i = 0; i < matchingPlayers.Count(); i++ )
			{
				UTIL_PrintToClient( pSender, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	CUtlString messageText;
	for ( int i = 3; i < args.ArgC(); ++i )
	{
		messageText.Append( args[ i ] );
		if ( i < args.ArgC() - 1 )
		{
			messageText.Append( " " );
		}
	}

	CUtlString formattedMessage;
	if ( isServerConsole )
	{
		formattedMessage = UTIL_VarArgs( "\x04[PRIVATE] Console: \x01%s\n", messageText.Get() );
	}
	else
	{
		formattedMessage = UTIL_VarArgs( "\x04[PRIVATE] %s: \x01%s\n", pSender->GetPlayerName(), messageText.Get() );
	}

	UTIL_PrintToClient( pTarget, formattedMessage.Get() );

	if ( pTarget != pSender )
	{
		if ( isServerConsole )
		{
			Msg( "Private message sent to %s: %s\n", pTarget->GetPlayerName(), messageText.Get() );
		}
		else
		{
			UTIL_PrintToClient( pSender, UTIL_VarArgs( "\x04[PRIVATE] %s: \x01%s\n", pSender->GetPlayerName(), messageText.Get() ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the admins list
//-----------------------------------------------------------------------------
static void ReloadAdminsCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	// only by a player or server console!!!
	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	// check permission
	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "i") )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	// reboot system...
	CHL2MP_Admin::InitAdminSystem();

	if ( isServerConsole )
	{
		Msg( "Admins list has been reloaded.\n" );
	}
	else
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Admins list has been reloaded.\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Help commands
//-----------------------------------------------------------------------------
static void PrintAdminHelp( CBasePlayer* pPlayer )
{
	if (!sv_showadminpermissions.GetBool() || CHL2MP_Admin::IsPlayerAdmin( pPlayer, "z" ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "[Server Admin] Usage: sa <command> [argument]\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "===== Admin Commands =====\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  say <message> -> Sends an admin formatted message to all players in the chat\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  csay <message> -> Sends a centered message to all players\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  psay <name|#userID> <message> -> Sends a private message to a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  chat <message> -> Sends a chat message to connected admins only\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  kick <name|#userID> [reason] -> Kick a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  ban <name|#userID> <time> [reason] -> Ban a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  addban <time> <SteamID3> [reason] -> Add a manual ban to banned_user.cfg\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  unban <SteamID3> -> Remove a banned SteamID from banned_user.cfg\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  slap <name|#userID> [amount] -> Slap a player with damage if defined\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  slay <name|#userID> -> Slay a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  noclip <name|#userID> -> Toggle noclip mode for a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  team <name|#userID> <team index> -> Move a player to another team\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  gag <name|#userID> -> Gag a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  ungag <name|#userID> -> Ungag a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  mute <name|#userID> -> Mute a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  unmute <name|#userID> -> Unmute a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  bring <name|#userID> -> Teleport a player to where an admin is aiming\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  goto <name|#userID> -> Teleport yourself to a player\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  map <map name> -> Change the map\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  cvar <cvar name> [new value] -> Modify any cvar's value\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  exec <filename> -> Executes a configuration file\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  rcon <command> [value] -> Send a command as if it was written in the server console\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  reloadadmins -> Refresh the admin cache\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  help -> Provide instructions on how to use the admin interface\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  credits -> Display credits\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  version -> Display version\n\n" );
	}
	else
	{
		// print what an admin has access to based on an admin's level
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "[Server Admin] Usage: sa <command> [argument]\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "===== Admin Commands =====\n" );
		if ( !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "z" ) )
		{
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "b" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  help -> Provide instructions on how to use the admin interface\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  credits -> Display credits\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  version -> Display version\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "c" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  kick <name|#userID> [reason] -> Kick a player\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "d" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  ban <name|#userID> <time> [reason] -> Ban a player\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "e" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  unban <SteamID3> -> Remove a banned SteamID from banned_user.cfg\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "f" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  slap <name|#userID> [amount] -> Slap a player with damage if defined\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  slay <name|#userID> -> Slay a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  noclip <name|#userID> -> Toggle noclip mode for a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  team <name|#userID> <team index> -> Move a player to another team\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  bring <name|#userID> -> Teleport a player to where an admin is aiming\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  goto <name|#userID> -> Teleport yourself to a player\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "g" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  map <map name> -> Change the map\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "h" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  cvar <cvar name> [new value] -> Modify any cvar's value\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "i" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  exec <filename> -> Executes a configuration file\n" );
			}
			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "j" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  gag <name|#userID> -> Gag a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  ungag <name|#userID> -> Ungag a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  mute <name|#userID> -> Mute a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  unmute <name|#userID> -> Unmute a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  say <message> -> Sends an admin formatted message to all players in the chat\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  csay <message> -> Sends a centered message to all players\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  psay <name|#userID> <message> -> Sends a private message to a player\n" );
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  chat <message> -> Sends a chat message to connected admins only\n" );
			}

			/*k permissions for voting later*/

			if ( CHL2MP_Admin::IsPlayerAdmin( pPlayer, "m" ) )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "  rcon <command> [value] -> Send a command as if it was written in the server console\n" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Display version
//-----------------------------------------------------------------------------
static void VersionCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( !pPlayer && isServerConsole )
	{
		Msg( "===== SERVER VERSION INFO =====\n" );
		Msg( "Server Admin version %s\n", SA_VERSION );
		Msg( "Powered by: %s\n\n", SA_POWERED );
	}
	else if ( pPlayer && !isServerConsole )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "===== SERVER VERSION INFO =====\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Server Admin version %s\n", SA_VERSION ) );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Powered by: %s\n\n", SA_POWERED ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static function to display credits
//-----------------------------------------------------------------------------
static void CreditsCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( !pPlayer && isServerConsole )
	{
		Msg( "===== SERVER ADMIN CREDITS =====\n" );
		Msg( "Server admin functionality -> Peter Brev\n" );
		Msg( "Source SDK provided by -> Valve\n\n" );
		Msg( "===== SPECIAL THANKS =====\n" );
		Msg( "Providing assistance and code fixes -> Potato\n" );
		Msg( "Providing assistance -> Tripperful\n" );
		Msg( "Providing assistance -> Tsmc\n" );
		Msg( "Past assistance -> Toizy\n\n" );
		Msg( "To all the following people without whom those binaries would not have been possible:\n" );
		Msg( "[GR]Ant_8490{A}\n" );
		Msg( "CLANG-CLANG\n" );
		Msg( "Harper\n" );
		Msg( "Henky\n" );
		Msg( "Humam\n" );
		Msg( "[MO] Kakujitsu\n" );
		Msg( "SALO POWER\n" );
		Msg( "Sub-Zero\n" );
		Msg( "Xeogin\n" );
		Msg( "Special thanks to all the HL2DM players keeping the game alive!\n\n" );
	}
	else if ( pPlayer && !isServerConsole )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "===== SERVER ADMIN CREDITS =====\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Server admin functionality -> Peter Brev\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Source SDK provided by -> Valve\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "===== SPECIAL THANKS =====\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Providing assistance and code fixes -> Potato\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Providing assistance -> Tripperful\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Providing assistance -> Tsmc\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Past assistance -> Toizy\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "To all the following people without whom those binaries would not have been possible:\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "[GR]Ant_8490{A}\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "CLANG-CLANG\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Harper\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Henky\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Humam\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "[MO] Kakujitsu\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "SALO POWER\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Sub-Zero\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Xeogin\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Special thanks to all the HL2DM players keeping the game alive!\n\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Help instructions for the admin interface
//-----------------------------------------------------------------------------
static void HelpPlayerCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "b" ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "You do not have permission to use this command.\n" );
		return;
	}

	if ( isServerConsole )
	{
		Msg( "===== SERVER ADMIN USAGE =====\n"
			"\n"
			"You can view all the available commands by typing \"sa\" into the console.\n"
			"Available commands will be different if the server or client console is used. All commands must start with \"sa\"\n"
			"\n"
			"You can use partial player names or a player's user ID. You can type \"status\" into your console\n"
			"to view the list of available players and retrive their user ID. You can then target them by doing \"sa ban #2 0\"\n"
			"The number sign (#) is required if you intend to target by user ID.\n"
			"\n"
			"Admin commands do not require the usage of quotes most of the time.\n"
			"You may need to use quotes around the player's name if you intend to use whitespaces or for exact name matching.\n"
			"The reason argument does not need quotes.\n"
			"Examples:\n"
			"  sa ban Pet 0 I banned you\n"
			"  sa ban \"Peter Brev\" 0 I banned you\n"
			"  sa ban #2 0 I banned you\n"
			"\n"
			"Note that special group targets take priority: \"sa ban @all 0\" will ban every player, even if a player is named @all.\n"
			"Use their userID to target such players.\n\n");
		Msg( "At any point in time you can type a command's name without additional arguments to view its syntax.\n"
			"Typing \"sa ban\" will print \"Usage: sa ban <name|#userid> <time> [reason]\"\n"
			"An argument between angled brackets means this argument is required, and an argument between square brackets it is not.\n"
			"For example, \"sa ban <name|#userid> <time> [reason]\" requires a name or userID and a time.\n"
			"\n"
			"You can target multiple players at once with the following:\n"
			"  @all -> will target all players\n"
			"  @me -> will target yourself\n"
			"  @blue -> will target the Combine team\n"
			"  @red -> will target the Rebels team\n"
			"  @!me -> will target everyone except yourself\n"
			"  @alive -> will target all the live players\n"
			"  @dead -> will target all the dead players\n"
			"  @bots -> will target all the bots\n"
			"  @humans -> will target all the human players\n\n" );
	}
	else
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"===== SERVER ADMIN USAGE =====\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"You can view all the available commands by typing \"sa\" into the console.\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"Available commands will be different if the server or client console is used. All commands must start with \"sa\"\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"You can use partial player names or a player's user ID. You can type \"status\" into your console\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"to view the list of available players and retrive their user ID. You can then target them by doing \"sa ban #2 0\"\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"The number sign (#) is required if you intend to target by user ID.\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"Admin commands do not require the usage of quotes most of the time.\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"You may need to use quotes around the player's name if you intend to use whitespaces or for exact name matching.\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"Examples:\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  sa ban Pet 0 I banned you\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  sa ban \"Peter Brev\" 0 I banned you\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  sa ban #2 0 I banned you\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"The reason argument does not need quotes.\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"Note that special group targets take priority: \"sa ban @all 0\" will ban every player, even if a player is named @all.\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"Use their userID to target such players.\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"At any point in time you can type a command's name without additional arguments to view its syntax.\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"Typing \"sa ban\" will print \"Usage: sa ban <name|#userid> <time> [reason]\"\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"An argument between angled brackets means this argument is required, and an argument between square brackets it is not.\n\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"You can target multiple players at once with the following:\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @all -> will target all players\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @blue -> will target the Combine team\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @red -> will target the Rebels team\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @!me -> will target everyone except yourself\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @alive -> will target all the live players\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @dead -> will target all the dead players\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @bots -> will target all the bots\n" );
		ClientPrint( pPlayer, HUD_PRINTCONSOLE,
			"  @humans -> will target all the human players\n\n" );
	}
	return;
}

bool CHL2MP_Admin::FindSpecialTargetGroup( const char* targetSpecifier )
{
	// magic targets
	bAll = false;
	bBlue = false;
	bRed = false;
	bAllButMe = false;
	bMe = false;
	bAlive = false;
	bDead = false;
	bBots = false;
	bHumans = false;

	if ( Q_stricmp( targetSpecifier, "@all" ) == 0 )
	{
		bAll = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@blue" ) == 0 )
	{
		bBlue = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@red" ) == 0 )
	{
		bRed = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@!me" ) == 0 )
	{
		bAllButMe = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@me" ) == 0 )
	{
		bMe = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@alive" ) == 0 )
	{
		bAlive = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@dead" ) == 0 )
	{
		bDead = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@bots" ) == 0 )
	{
		bBots = true;
		return true;
	}
	else if ( Q_stricmp( targetSpecifier, "@humans" ) == 0 )
	{
		bHumans = true;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check for any human players
//-----------------------------------------------------------------------------
static bool ArePlayersInGame()
{
	// mainly for the change level admin command
	// if no player connected to the server,
	// the time remains frozen, therefore the level
	// never changes; this makes it so that if no
	// players are connected, it changes the level
	// immediately rather than after 5 seconds
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Execute files
//-----------------------------------------------------------------------------
static void ExecFileCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "i" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa exec <filename>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa exec <filename>\n" );
		}
		return;
	}

	const char* filename = args.Arg( 2 );

	engine->ServerCommand( UTIL_VarArgs( "exec %s\n", filename ) );

	if ( isServerConsole )
	{
		Msg( "Executing config file: %s\n", filename );
	}
	else
	{
		UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Executing config file: " CHAT_DEFAULT "%s\n", filename ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Toggle noclip
//-----------------------------------------------------------------------------
static void ToggleNoClipForPlayer( CBasePlayer* pTarget, CBasePlayer* pAdmin )
{
	if ( pTarget->GetMoveType() == MOVETYPE_NOCLIP )
	{
		pTarget->SetMoveType( MOVETYPE_WALK );
	}
	else
	{
		pTarget->SetMoveType( MOVETYPE_NOCLIP );
	}
}

static void NoClipPlayerCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();
	
	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "f" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa noclip <name|#userID>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa noclip <name|#userID>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	// special group targeting
	if ( targetPlayerInput[ 0 ] == '@' )
	{
		if ( HL2MPAdmin()->FindSpecialTargetGroup( targetPlayerInput ) )
		{
			// not the dead players
			if ( Q_stricmp( targetPlayerInput, "@dead" ) == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "Can't target dead players.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "Can't target dead players.\n" );
				}
				return;
			}
			else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				if ( isServerConsole )
					Msg( "Can't use @me from the server console\n" );
				else
					UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @me from the server console\n" );
				return;
			}
			else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				if ( isServerConsole )
					Msg( "Can't use @!me from the server console\n" );
				else
					UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @!me from the server console\n" );
			}

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( !pLoopPlayer || !pLoopPlayer->IsPlayer() )
				{
					continue;
				}

				if ( pLoopPlayer->IsAlive() && ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pLoopPlayer->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pLoopPlayer->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pLoopPlayer != pAdmin ) ||
					( HL2MPAdmin()->IsMe() && pLoopPlayer == pAdmin ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pLoopPlayer->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pLoopPlayer->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pLoopPlayer->IsBot() ) ) )
				{
					targetPlayers.AddToTail( pLoopPlayer );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		// the player name requires quote for exact name matching 
		// or if two names start with the same prefix e.g. I Am Groot / I Am Groot More
		// I may revisit this to allow admins to add a character before the player name
		// for exact name matching ($name, &name, ##name)
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}
		pTarget = matchingPlayers[ 0 ];
	}

	if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			ToggleNoClipForPlayer( targetPlayers[ i ], pAdmin );
		}

		if ( isServerConsole )
		{
			Msg( "Toggled noclip for players in group %s.\n", targetPlayerInput + 1 );  // skip the '@'
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "toggled noclip for all players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", targetPlayerInput + 1 ) );
		}
		else
		{
			if ( Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "toggled noclip for themself.\n", pAdmin->GetPlayerName() ) );
			}
			else if ( Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "toggled noclip for all players except themself.\n", pAdmin->GetPlayerName() ) );
			}
			else
			{		
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "toggled noclip for players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pAdmin->GetPlayerName(), targetPlayerInput + 1 ) );
			}
		}
	}
	else if ( pTarget )
	{
		if (!pTarget->IsAlive())
		{
			UTIL_PrintToClient(pAdmin, UTIL_VarArgs(CHAT_RED "This player is currently dead.\n", pTarget->GetPlayerName()));
			return;
		}

		ToggleNoClipForPlayer( pTarget, pAdmin );
		
		if ( isServerConsole )
		{
			Msg( "Console toggled noclip for player %s\n", pTarget->GetPlayerName() );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "toggled noclip for " CHAT_DEFAULT "%s\n", pTarget->GetPlayerName()));
		}
		else
		{
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "toggled noclip for " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName()));
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Teleport to a player
//-----------------------------------------------------------------------------
static void GotoPlayerCommand(const CCommand& args)
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();

	if (!pAdmin)
	{
		UTIL_PrintToClient(pAdmin, CHAT_RED "Command must be issued by a player.\n");
		return;
	}

	if (!CHL2MP_Admin::IsPlayerAdmin(pAdmin, "f"))
	{
		UTIL_PrintToClient(pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n");
		return;
	}

	if (args.ArgC() < 3)
	{
		UTIL_PrintToClient(pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa goto <name|#userID>\n");
		return;
	}

	const char* targetPlayerInput = args.Arg(2);
	CBasePlayer* pTarget = nullptr;

	if (targetPlayerInput[0] == '#')
	{
		int userID = atoi(&targetPlayerInput[1]);
		if (userID > 0)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex(i);
				if (pLoopPlayer && pLoopPlayer->GetUserID() == userID)
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if (!pTarget)
			{
				UTIL_PrintToClient(pAdmin, CHAT_RED "No player found with that UserID.\n");
				return;
			}
		}
		else
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "Invalid UserID provided.\n");
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex(i);
			if (pLoopPlayer && Q_stristr(pLoopPlayer->GetPlayerName(), targetPlayerInput))
			{
				matchingPlayers.AddToTail(pLoopPlayer);
			}
		}

		if (matchingPlayers.Count() == 0)
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "No players found matching that name.\n");
			return;
		}
		else if (matchingPlayers.Count() > 1)
		{
			UTIL_PrintToClient(pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n");
			for (int i = 0; i < matchingPlayers.Count(); i++)
			{
				UTIL_PrintToClient(pAdmin, UTIL_VarArgs(CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[i]->GetPlayerName()));
			}
			return;
		}

		pTarget = matchingPlayers[0];
	}

	if (pTarget)
	{
		if (!pTarget->IsAlive())
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "This player is currently dead.\n");
			return;
		}

		if (pAdmin->IsAlive())
		{
			Vector targetPosition = pTarget->GetAbsOrigin();
			targetPosition.z += 80.0f;

			pAdmin->SetAbsOrigin(targetPosition);

			UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "teleported to " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName()));
		}
		else
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "You must be alive to teleport to a player.\n");
		}
	}
	else
	{
		UTIL_PrintToClient(pAdmin, CHAT_RED "Player not found.\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Teleport players to where an admin is aiming
//-----------------------------------------------------------------------------
static void BringPlayerCommand(const CCommand& args)
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();

	if (!pAdmin)
	{
		UTIL_PrintToClient(pAdmin, CHAT_RED "Command must be issued by a player.\n");
		return;
	}

	if (!CHL2MP_Admin::IsPlayerAdmin(pAdmin, "f"))
	{
		UTIL_PrintToClient(pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n");
		return;
	}

	if (args.ArgC() < 3)
	{
		UTIL_PrintToClient(pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa bring <name|#userID>\n");
		return;
	}

	const char* targetPlayerInput = args.Arg(2);
	CBasePlayer* pTarget = nullptr;

	if (Q_stricmp(targetPlayerInput, "@me") == 0)
	{
		pTarget = pAdmin;
	}

	else if (targetPlayerInput[0] == '#')
	{
		int userID = atoi(&targetPlayerInput[1]);
		if (userID > 0)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex(i);
				if (pLoopPlayer && pLoopPlayer->GetUserID() == userID)
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if (!pTarget)
			{
				UTIL_PrintToClient(pAdmin, CHAT_RED "No player found with that UserID.\n");
				return;
			}
		}
		else
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "Invalid UserID provided.\n");
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex(i);
			if (pLoopPlayer && Q_stristr(pLoopPlayer->GetPlayerName(), targetPlayerInput))
			{
				matchingPlayers.AddToTail(pLoopPlayer);
			}
		}

		if (matchingPlayers.Count() == 0)
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "No players found matching that name.\n");
			return;
		}
		else if (matchingPlayers.Count() > 1)
		{
			UTIL_PrintToClient(pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n");
			for (int i = 0; i < matchingPlayers.Count(); i++)
			{
				UTIL_PrintToClient(pAdmin, UTIL_VarArgs(CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[i]->GetPlayerName()));
			}
			return;
		}

		pTarget = matchingPlayers[0];
	}

	if (pTarget)
	{
		if (pAdmin->IsAlive())
		{
			Vector forward;
			trace_t tr;

			pAdmin->EyeVectors(&forward);
			UTIL_TraceLine(pAdmin->EyePosition(), pAdmin->EyePosition() + forward * MAX_COORD_RANGE, MASK_SOLID, pAdmin, COLLISION_GROUP_NONE, &tr);

			Vector targetPosition = tr.endpos;
			pTarget->SetAbsOrigin(targetPosition);

			UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "teleported player " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName()));
		}
		else
		{
			UTIL_PrintToClient(pAdmin, CHAT_RED "You must be alive to teleport a player.\n");
		}
	}
	else
	{
		UTIL_PrintToClient(pAdmin, CHAT_RED "Player not found.\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Change a player's team
//-----------------------------------------------------------------------------
static void TeamPlayerCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "f" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 4 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa team <name|#userID> <team index>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa team <name|#userID> <team index>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	int teamIndex = atoi( args.Arg( 3 ) );

	if ( teamIndex < 1 || teamIndex > 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Invalid team index. Team index must be between 1 and 3.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid team index. Team index must be between 1 and 3.\n" );
		}
		return;
	}

	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	// Handle special group targets
	if ( targetPlayerInput[ 0 ] == '@' )
	{
		if ( isServerConsole && Q_stricmp( targetPlayerInput, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( targetPlayerInput ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( !pLoopPlayer || !pLoopPlayer->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pLoopPlayer->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pLoopPlayer->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pLoopPlayer != pAdmin ) ||
					( HL2MPAdmin()->IsMe() && pLoopPlayer == pAdmin ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pLoopPlayer->IsAlive() ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pLoopPlayer->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pLoopPlayer->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pLoopPlayer->IsBot() ) )
				{
					targetPlayers.AddToTail( pLoopPlayer );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	const char* teamName = "Players";
	if ( teamIndex == 1 )
	{
		teamName = "Spectator";
	}
	else if ( HL2MPRules()->IsTeamplay() )
	{
		if ( teamIndex == 2 )
		{
			teamName = "Combine";
		}
		else if ( teamIndex == 3 )
		{
			teamName = "Rebels";
		}
	}

	if ( pTarget )
	{
		if ( pTarget->GetTeamNumber() == teamIndex )
		{
			if ( isServerConsole )
			{
				Msg( "Player %s is already on team %s.\n", pTarget->GetPlayerName(), teamName );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN "Player " CHAT_DEFAULT "%s " CHAT_ADMIN "is already on team " CHAT_DEFAULT "%s.\n", pTarget->GetPlayerName(), teamName ) );
			}
			return;
		}
		// team Unassigned is index 0, so we need to separately check for it
		else if ( !HL2MPRules()->IsTeamplay() && pTarget->GetTeamNumber() == TEAM_UNASSIGNED )
		{
			if ( isServerConsole )
			{
				Msg( "Player %s is already on team %s.\n", pTarget->GetPlayerName(), teamName );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN "Player " CHAT_DEFAULT "%s " CHAT_ADMIN "is already on team " CHAT_DEFAULT "%s.\n", pTarget->GetPlayerName(), teamName ) );
			}
			return;
		}

		pTarget->ChangeTeam( teamIndex );
		CUtlString sTeamMessage;

		if (isServerConsole)
		{
			Msg("Console moved player %s to team %s.\n", pTarget->GetPlayerName(), teamName);
			sTeamMessage = UTIL_VarArgs(CHAT_DEFAULT "Console " CHAT_ADMIN "moved player " CHAT_DEFAULT "%s " CHAT_ADMIN "to team " CHAT_DEFAULT "%s.\n", pTarget->GetPlayerName(), teamName);
		}
		else
		{
			sTeamMessage = UTIL_VarArgs(
				CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "moved player " CHAT_DEFAULT "%s " CHAT_ADMIN "to team " CHAT_DEFAULT "%s.\n",
				pAdmin->GetPlayerName(), pTarget->GetPlayerName(), teamName);
		}

		UTIL_PrintToAllClients(sTeamMessage.Get());

	}
	else if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			pTarget->ChangeTeam( teamIndex );
		}

		if ( isServerConsole )
		{
			Msg( "Moved players in group %s to team %s.\n", targetPlayerInput + 1, teamName );  // Skip the '@'
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "moved players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "to team " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", targetPlayerInput + 1, teamName ) );
		}
		else
		{
			if ( Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "moved themself to team " CHAT_DEFAULT "%s.\n", pAdmin->GetPlayerName(), teamName ) );
			}
			else if ( Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "moved all players except themself to team " CHAT_DEFAULT "%s.\n", pAdmin->GetPlayerName(), teamName ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "moved players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "to team " CHAT_DEFAULT "%s.\n", pAdmin->GetPlayerName(), targetPlayerInput + 1, teamName ) );
			}
		}
	}

	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Unmute a player
//-----------------------------------------------------------------------------
static void UnMutePlayerCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "j" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa unmute <name|#userID>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa unmute <name|#userID>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	if ( targetPlayerInput[ 0 ] == '@' )
	{
		if ( HL2MPAdmin()->FindSpecialTargetGroup( targetPlayerInput ) )
		{
			if ( isServerConsole && Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				if ( isServerConsole )
					Msg( "Can't use @me from the server console\n" );
				else
					UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @me from the server console\n" );
				return;
			}
			else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				if ( isServerConsole )
					Msg( "Can't use @!me from the server console\n" );
				else
					UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @!me from the server console\n" );
			}

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pAdmin ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pAdmin ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( pTarget )
	{
		const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

		CHL2MP_Admin* pAdminTarget = CHL2MP_Admin::GetAdmin( targetSteamID );

		// not on a root admin!
		if ( pAdminTarget != nullptr && pAdminTarget->HasPermission( ADMIN_ROOT ) && ( pTarget != pAdmin ) && !isServerConsole )
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Cannot target this player (root admin privileges).\n" );
			return;
		}

		if ( !pTarget->IsMuted() )
		{
			if ( isServerConsole )
			{
				Msg( "Player %s is not muted.\n", pTarget->GetPlayerName() );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Player is not muted.\n" );
			}
			return;
		}

		pTarget->SetMuted( false );

		if ( isServerConsole )
		{
			Msg( "Player %s has been unmuted.\n", pTarget->GetPlayerName() );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "has unmuted " CHAT_DEFAULT "%s\n", pTarget->GetPlayerName() ) );
		}
		else
		{
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "has unmuted " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName() ) );
		}
	}
	else if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			if ( pTarget->IsMuted() )
			{
				pTarget->SetMuted( false );
			}
		}

		if ( isServerConsole )
		{
			Msg( "Unmuted players in group %s.\n", targetPlayerInput + 1 );  // Skip the '@'
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "unmuted all players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", targetPlayerInput + 1 ) );
		}
		else
		{
			if ( Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "unmuted themself.\n", pAdmin->GetPlayerName() ) );
			}
			else if ( Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "unmuted all players except themself.\n", pAdmin->GetPlayerName() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "unmuted players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pAdmin->GetPlayerName(), targetPlayerInput + 1 ) );  // Skip the '@'
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Mute player
//-----------------------------------------------------------------------------
static void MutePlayerCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "j" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa mute <name|#userID>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa mute <name|#userID>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	if ( targetPlayerInput[ 0 ] == '@' )
	{
		if ( isServerConsole && Q_stricmp( targetPlayerInput, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( targetPlayerInput ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pAdmin ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pAdmin ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( pTarget )
	{
		const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

		CHL2MP_Admin* pAdminTarget = CHL2MP_Admin::GetAdmin( targetSteamID );

		if ( pAdminTarget != nullptr && pAdminTarget->HasPermission( ADMIN_ROOT ) && ( pTarget != pAdmin ) && !isServerConsole )
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Cannot target this player (root admin privileges).\n" );
			return;
		}

		if ( pTarget->IsMuted() )
		{
			if ( isServerConsole )
			{
				Msg( "Player %s is already muted.\n", pTarget->GetPlayerName() );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Player is already muted.\n" );
			}
			return;
		}

		pTarget->SetMuted( true );

		if ( isServerConsole )
		{
			Msg( "Player %s has been muted.\n", pTarget->GetPlayerName() );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "has muted " CHAT_DEFAULT "%s\n", pTarget->GetPlayerName() ) );
		}
		else
		{
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "has muted " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName() ) );
		}
	}
	else if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			if ( !pTarget->IsMuted() )
			{
				pTarget->SetMuted( true );
			}
		}

		if ( isServerConsole )
		{
			Msg( "Muted players in group %s.\n", targetPlayerInput + 1 );  // Skip the '@'
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "muted all players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", targetPlayerInput + 1 ) );
		}
		else
		{
			if ( Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "muted themself.\n", pAdmin->GetPlayerName() ) );
			}
			else if ( Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "muted all players except themself.\n", pAdmin->GetPlayerName() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "muted players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pAdmin->GetPlayerName(), targetPlayerInput + 1 ) );  // Skip the '@'
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ungag player
//-----------------------------------------------------------------------------
static void UnGagPlayerCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "j" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa ungag <name|#userID>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa ungag <name|#userID>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	if ( targetPlayerInput[ 0 ] == '@' )
	{
		if ( isServerConsole && Q_stricmp( targetPlayerInput, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( targetPlayerInput ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pAdmin ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pAdmin ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( pTarget )
	{
		const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

		CHL2MP_Admin* pAdminTarget = CHL2MP_Admin::GetAdmin( targetSteamID );

		if ( pAdminTarget != nullptr && pAdminTarget->HasPermission( ADMIN_ROOT ) && ( pTarget != pAdmin ) && !isServerConsole )
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Cannot target this player (root admin privileges).\n" );
			return;
		}

		if ( !pTarget->IsGagged() )
		{
			if ( isServerConsole )
			{
				Msg( "Player %s is not gagged.\n", pTarget->GetPlayerName() );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Player is not gagged.\n" );
			}
			return;
		}

		pTarget->SetGagged( false );

		if ( isServerConsole )
		{
			Msg( "Player %s has been ungagged.\n", pTarget->GetPlayerName() );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "has ungagged " CHAT_DEFAULT "%s\n", pTarget->GetPlayerName() ) );
		}
		else
		{
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "has ungagged " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName() ) );
		}
	}
	else if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			if ( pTarget->IsGagged() )
			{
				pTarget->SetGagged( false );
			}
		}

		if ( isServerConsole )
		{
			Msg( "Ungagged players in group %s.\n", targetPlayerInput + 1 );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "ungagged all players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", targetPlayerInput + 1 ) );
		}
		else
		{
			if ( Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "ungagged themself.\n", pAdmin->GetPlayerName() ) );
			}
			else if ( Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "ungagged all players except themself.\n", pAdmin->GetPlayerName() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "ungagged players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pAdmin->GetPlayerName(), targetPlayerInput + 1 ) );  // Skip the '@'
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gag player
//-----------------------------------------------------------------------------
static void GagPlayerCommand( const CCommand& args )
{
	CBasePlayer* pAdmin = UTIL_GetCommandClient();
	bool isServerConsole = !pAdmin && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pAdmin && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pAdmin && !CHL2MP_Admin::IsPlayerAdmin( pAdmin, "j" ) )
	{
		UTIL_PrintToClient( pAdmin, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa gag <name|#userID>\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa gag <name|#userID>\n" );
		}
		return;
	}

	const char* targetPlayerInput = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	if ( targetPlayerInput[ 0 ] == '@' )
	{
		if ( isServerConsole && Q_stricmp( targetPlayerInput, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pAdmin, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( targetPlayerInput ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pAdmin ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pAdmin ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( targetPlayerInput[ 0 ] == '#' )
	{
		int userID = atoi( &targetPlayerInput[ 1 ] ); 
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID\n" );
				}
				else
				{
					UTIL_PrintToClient( pAdmin, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), targetPlayerInput ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name\n" );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pAdmin, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( pTarget )
	{
		const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

		CHL2MP_Admin* pAdminTarget = CHL2MP_Admin::GetAdmin( targetSteamID );

		if ( pAdminTarget != nullptr && pAdminTarget->HasPermission( ADMIN_ROOT ) && ( pTarget != pAdmin ) && !isServerConsole )
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Cannot target this player (root admin privileges).\n" );
			return;
		}

		if ( pTarget->IsGagged() )
		{
			if ( isServerConsole )
			{
				Msg( "Player %s is already gagged.\n", pTarget->GetPlayerName() );
			}
			else
			{
				UTIL_PrintToClient( pAdmin, CHAT_RED "Player is already gagged.\n" );
			}
			return;
		}

		pTarget->SetGagged( true );

		if ( isServerConsole )
		{
			Msg( "Player %s has been gagged.\n", pTarget->GetPlayerName() );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "has gagged " CHAT_DEFAULT "%s\n", pTarget->GetPlayerName() ) );
		}
		else
		{
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "has gagged " CHAT_DEFAULT "%s\n", pAdmin->GetPlayerName(), pTarget->GetPlayerName() ) );
		}
	}
	else if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			if ( !pTarget->IsGagged() )
			{
				pTarget->SetGagged( true );
			}
		}

		if ( isServerConsole )
		{
			Msg( "Gagged players in group %s\n", targetPlayerInput + 1 );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "gagged all players in group " CHAT_DEFAULT "%s.\n", targetPlayerInput + 1 ) );
		}
		else
		{
			if ( Q_stricmp( targetPlayerInput, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "gagged themself.\n", pAdmin->GetPlayerName() ) );
			}
			else if ( Q_stricmp( targetPlayerInput, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "gagged all players except themself.\n", pAdmin->GetPlayerName() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "gagged players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pAdmin->GetPlayerName(), targetPlayerInput + 1 ) );  // Skip the '@'
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pAdmin, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Change map
//-----------------------------------------------------------------------------
static void MapCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();  // Check if command issued by server console

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "g" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa map <mapname>\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa map <mapname>\n" );
		}
		return;
	}

	// don't change map if we are about to change anyway
	if ( HL2MPRules()->IsMapChangeOnGoing() )
	{
		if ( isServerConsole )
		{
			Msg( "A map change is already in progress...\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "A map change is already in progress...\n" );
		}
		return;
	}

	const char* partialMapName = args.Arg( 2 );

	CUtlVector<char*> matchingMaps;

	FileFindHandle_t fileHandle;
	const char* mapPath = filesystem->FindFirst( "maps/*.bsp", &fileHandle );

	char* exactMatchMap = nullptr;

	while ( mapPath )
	{

		char mapName[ 256 ];
		V_FileBase( mapPath, mapName, sizeof( mapName ) );

		// do we have an exact map name match?
		if ( Q_stricmp( mapName, partialMapName ) == 0 )
		{
			exactMatchMap = new char[ Q_strlen( mapName ) + 1 ];
			Q_strncpy( exactMatchMap, mapName, Q_strlen( mapName ) + 1 );
			break;
		}

		// if not, check for a partial name
		if ( Q_stristr( mapName, partialMapName ) )
		{
			char* mapNameCopy = new char[ Q_strlen( mapName ) + 1 ];
			Q_strncpy( mapNameCopy, mapName, Q_strlen( mapName ) + 1 );
			matchingMaps.AddToTail( mapNameCopy );
		}

		mapPath = filesystem->FindNext( fileHandle );
	}

	filesystem->FindClose( fileHandle );

	if ( exactMatchMap )
	{
		// if nobody is in the game, change the map now
		if ( !ArePlayersInGame() )
		{
			engine->ServerCommand( UTIL_VarArgs( "changelevel %s\n", exactMatchMap ) );
			delete[] exactMatchMap;
			return;
		}

		HL2MPRules()->SetScheduledMapName( exactMatchMap );  // change map in 5 seconds
		HL2MPRules()->SetMapChange( true );
		HL2MPRules()->SetMapChangeOnGoing( true );

		if ( isServerConsole )
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "is changing the map to " CHAT_DEFAULT "%s" CHAT_ADMIN " in 5 seconds...\n", exactMatchMap));
		else
			UTIL_PrintToAllClients( UTIL_VarArgs(CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "is changing the map to " CHAT_DEFAULT "%s" CHAT_ADMIN " in 5 seconds...\n", pPlayer->GetPlayerName(), exactMatchMap));
		engine->ServerCommand( "mp_timelimit 0\n" );

		delete[] exactMatchMap;
		return;
	}

	if ( matchingMaps.Count() == 0 )
	{
		if ( isServerConsole )
		{
			Msg( "No maps found matching \"%s\".\n", partialMapName );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_RED "No maps found matching " CHAT_DEFAULT "%s\n", partialMapName ) );
		}
		return;
	}

	if ( matchingMaps.Count() == 1 )
	{
		if ( !ArePlayersInGame() )
		{
			engine->ServerCommand( UTIL_VarArgs( "changelevel %s\n", matchingMaps[ 0 ] ) );
		}
		else
		{
			HL2MPRules()->SetScheduledMapName( matchingMaps[ 0 ] );
			HL2MPRules()->SetMapChange( true );
			HL2MPRules()->SetMapChangeOnGoing( true );

			if ( isServerConsole )
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "is changing the map to " CHAT_DEFAULT "%s" CHAT_ADMIN " in 5 seconds...\n", matchingMaps[0]));
			else
				UTIL_PrintToAllClients( UTIL_VarArgs(CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "is changing the map to " CHAT_DEFAULT " %s" CHAT_ADMIN " in 5 seconds...\n",  pPlayer->GetPlayerName(), matchingMaps[0]));
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Multiple maps match the partial name:\n" );
			for ( int i = 0; i < matchingMaps.Count(); i++ )
			{
				Msg( "%s\n", matchingMaps[ i ] );
			}
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Multiple maps match the partial name:\n" );
			for ( int i = 0; i < matchingMaps.Count(); i++ )
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingMaps[ i ] ) );
			}
		}
	}

	for ( int i = 0; i < matchingMaps.Count(); i++ )
	{
		delete[] matchingMaps[ i ];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Rcon
//-----------------------------------------------------------------------------
static void RconCommand( const CCommand& args )
{
	// For rcon, we are only making this command available to players in-game 
	// (meaning it will not do anything if used within the server console directly) 
	// because it makes no sense to use rcon in the server console, but we are also 
	// disabling the usage of all "sa" commands with "sa rcon" since if the admin has rcon, 
	// he probably has root privileges already

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "m" ) )  // "m" flag for RCON permission
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa rcon <command> [argument]\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa rcon <command> [argument]\n" ) );
		}
		return;
	}

	const char* commandName = args.Arg( 2 );

	if ( Q_stricmp( commandName, "sa") == 0 )
	{
		UTIL_PrintToClient(pPlayer, UTIL_VarArgs(CHAT_ADMIN "No " CHAT_DEFAULT "\"sa rcon\" " CHAT_ADMIN "needed with commands starting with " CHAT_DEFAULT "\"%s\"\n", commandName));
		return;
	}

	const ConCommandBase* pCommand = g_pCVar->FindCommand( commandName );
	ConVar* pConVar = g_pCVar->FindVar( commandName );

	if ( !pCommand && !pConVar )
	{
		if ( isServerConsole )
		{
			Msg( "Unknown command \"%s\"\n", commandName );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_RED "Unknown command \"" CHAT_DEFAULT "%s" CHAT_RED "\"\n", commandName ) );
		}
		return;
	}

	if ( pConVar && args.ArgC() == 3 )
	{
		float currentValue = pConVar->GetFloat();

		if ( fabs( currentValue - roundf( currentValue ) ) < 0.0001f )
		{
			int intValue = static_cast< int >( currentValue );
			if ( isServerConsole )
			{
				Msg( "Cvar %s is currently set to %d.\n", commandName, intValue );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " is currently set to " CHAT_DEFAULT "%d.\n", commandName, intValue ) );
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Cvar %s is currently set to %f.\n", commandName, currentValue );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "CVar " CHAT_DEFAULT "%s" CHAT_ADMIN " is currently set to " CHAT_DEFAULT "%f.\n", commandName, currentValue ) );
			}
		}
		return;
	}

	CUtlString rconCommand;
	for ( int i = 2; i < args.ArgC(); i++ )
	{
		rconCommand.Append( args.Arg( i ) );
		if ( i < args.ArgC() - 1 )
		{
			rconCommand.Append( " " );
		}
	}

	engine->ServerCommand( UTIL_VarArgs( "%s\n", rconCommand.Get() ) );

	if ( isServerConsole )
	{
		Msg( "Rcon command issued: %s\n", rconCommand.Get() );
	}
	else
	{
		UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Rcon command issued: " CHAT_DEFAULT "%s\n", rconCommand.Get() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cvar
//-----------------------------------------------------------------------------
static void CVarCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "h" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have permission to use this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa cvar <cvarname> [newvalue]\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa cvar <cvarname> [newvalue]\n" ) );
		}
		return;
	}

	const char* cvarName = args.Arg( 2 );

	ConVar* pConVar = cvar->FindVar( cvarName );
	if ( !pConVar )
	{
		if ( isServerConsole )
		{
			Msg( "Cvar %s not found.\n", cvarName );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_RED "Cvar " CHAT_DEFAULT "%s" CHAT_RED " not found.\n", cvarName ) );
		}
		return;
	}

	// if there is an sv_cheats dependency, 
	// ensure the player has the ADMIN_CHEAT flag if necessary
	bool requiresCheatFlag = pConVar->IsFlagSet(FCVAR_CHEAT);
	if (requiresCheatFlag && pPlayer && !CHL2MP_Admin::IsPlayerAdmin(pPlayer, "n"))
	{
		UTIL_PrintToClient(pPlayer, CHAT_RED "You do not have permission to change cheat protected cvars.\n");
		return;
	}

	if ( args.ArgC() == 3 )
	{
		float currentValue = pConVar->GetFloat();

		if ( fabs( currentValue - roundf( currentValue ) ) < 0.0001f )
		{
			int intValue = static_cast< int >( currentValue );
			if ( isServerConsole )
			{
				Msg( "Cvar %s is currently set to %d.\n", cvarName, intValue );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " is currently set to " CHAT_DEFAULT "%d.\n", cvarName, intValue ) );
			}
		}
		else
		{
			// don't want to print the password
			if ( isServerConsole )
			{
				Msg( "Cvar %s is currently set to %f.\n", cvarName, currentValue );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " is currently set to " CHAT_DEFAULT "%f.\n", cvarName, currentValue ) );
			}
		}
		return;
	}

	// modify or reset the value from here on out
	const char* newValue = args.Arg(3);

	if (Q_stricmp(newValue, "reset") == 0)
	{
		pConVar->Revert();
		UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_ADMIN "Cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " reset to default value.\n", cvarName));
		return;
	}

	// if we are trying to change the password, 
	// make sure we have the authorizations to do so
	if (Q_stricmp(cvarName, "sv_password") == 0)
	{
		if (pPlayer && !CHL2MP_Admin::IsPlayerAdmin(pPlayer, "l") && !isServerConsole)
		{
			UTIL_PrintToClient(pPlayer, CHAT_RED "You do not have permission to add or change the server password.\n");
			return;
		}

		if (Q_stricmp(pConVar->GetString(), newValue) == 0)
		{
			if (isServerConsole)
			{
				Msg("Cvar sv_password is already set to \"%s\".\n", newValue);
			}
			else
			{
				UTIL_PrintToClient(pPlayer, UTIL_VarArgs(CHAT_ADMIN "Cvar " CHAT_DEFAULT "sv_password" CHAT_ADMIN " is already set to " CHAT_DEFAULT "\"%s\".\n", newValue));
			}
			return;
		}

		pConVar->SetValue(newValue);

		return;
	}

	float f_currentValue = pConVar->GetFloat();
	float f_inputValue = atof( newValue );

	const float epsilon = 0.0001f;

	if ( fabs( f_currentValue - roundf( f_currentValue ) ) < epsilon )
	{
		int int_currentValue = static_cast< int >( f_currentValue );

		if ( fabs( f_inputValue - roundf( f_inputValue ) ) < epsilon )
		{
			int int_inputValue = static_cast< int >( f_inputValue );

			if ( int_currentValue == int_inputValue )
			{
				if ( isServerConsole )
				{
					Msg( "Cvar %s is already set to %d.\n", cvarName, int_currentValue );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " is already set to " CHAT_DEFAULT "%d.\n", cvarName, int_currentValue ) );
				}
				return;
			}
		}
	}

	if ( f_currentValue == f_inputValue )
	{
		if ( isServerConsole )
		{
			Msg( "Cvar %s is already set to %f.\n", cvarName, f_currentValue );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "Cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " is already set to " CHAT_DEFAULT "%f.\n", cvarName, f_currentValue ) );
		}
		return;
	}

	pConVar->SetValue( newValue );

	if ( isServerConsole )
	{
		Msg( "CVar %s set to %s.\n", cvarName, newValue );
		UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_DEFAULT "Console " CHAT_ADMIN "changed cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " set to " CHAT_DEFAULT "%s.\n", cvarName, newValue));
	}
	else
	{
		UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "changed cvar " CHAT_DEFAULT "%s" CHAT_ADMIN " set to " CHAT_DEFAULT "%s.\n", pPlayer->GetPlayerName(), cvarName, newValue));
	}
}

//-----------------------------------------------------------------------------
// Purpose: SLap player (+ damage)
//-----------------------------------------------------------------------------
static void SlapPlayerCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "f" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa slap <name|#userid> [amount]\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa slap <name|#userid> [amount]\n" );
		}
		return;
	}

	const char* partialName = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	if ( partialName[ 0 ] == '@' )
	{
		if ( Q_stricmp( partialName, "@dead" ) == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "Can't target dead players.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't target dead players.\n" );
			}
			return;
		}
		else if ( isServerConsole && Q_stricmp( partialName, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( partialName, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( partialName ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pPlayer ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pPlayer ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( partialName[ 0 ] == '#' )
	{
		int userID = atoi( &partialName[ 1 ] );  // Extract the number after '#'
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), partialName ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( pTarget )
	{
		if ( !pTarget->IsAlive() )
		{
			if ( isServerConsole )
			{
				Msg( "Cannot slap %s. This player is already dead.\n", pTarget->GetPlayerName() );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "This player is currently dead.\n" );
			}
			return;
		}

		int slapDamage = 0;
		if ( args.ArgC() > 3 )
		{
			slapDamage = atoi( args.Arg( 3 ) );
			if ( slapDamage < 0 )
			{
				slapDamage = 0;
			}
		}

		Vector slapForce;
		slapForce.x = RandomFloat( -150, 150 );
		slapForce.y = RandomFloat( -150, 150 );
		slapForce.z = RandomFloat( 200, 400 );

		pTarget->ApplyAbsVelocityImpulse( slapForce ); 

		if ( slapDamage > 0 )
		{
			CTakeDamageInfo damageInfo( pTarget, pTarget, slapDamage, DMG_FALL );
			pTarget->TakeDamage( damageInfo );
		}

		CBroadcastRecipientFilter filter;
		filter.AddRecipient( pTarget );
		filter.MakeReliable();

		int iRandomSnd = random->RandomInt( 1, 2 );

		if ( iRandomSnd == 1 )
			CBaseEntity::EmitSound( filter, pTarget->entindex(), "Player.FallDamage" );
		else
			CBaseEntity::EmitSound( filter, pTarget->entindex(), "Player.SonicDamage" );	

		if ( isServerConsole )
		{
			if ( slapDamage > 0 )
			{
				Msg( "Slapped %s for %d damage\n", pTarget->GetPlayerName(), slapDamage );
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "slapped " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d damage.\n", pTarget->GetPlayerName(), slapDamage ) );
			}
			else
			{
				Msg( "Slapped %s\n", pTarget->GetPlayerName() );
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "slapped " CHAT_DEFAULT "%s.\n", pTarget->GetPlayerName() ) );
			}
		}
		else
		{
			if ( slapDamage > 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "%s " CHAT_ADMIN "slapped " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d damage.\n", pPlayer->GetPlayerName(), pTarget->GetPlayerName(), slapDamage ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "%s " CHAT_ADMIN "slapped " CHAT_DEFAULT "%s.\n", pPlayer->GetPlayerName(), pTarget->GetPlayerName() ) );
			}
		}
	}
	else if ( targetPlayers.Count() > 0 )
	{
		int slapDamage = 0;
		if ( args.ArgC() > 3 )
		{
			slapDamage = atoi( args.Arg( 3 ) );
			if ( slapDamage < 0 )
			{
				slapDamage = 0;
			}
		}

		Vector slapForce;
		slapForce.x = RandomFloat( -150, 150 );
		slapForce.y = RandomFloat( -150, 150 );
		slapForce.z = RandomFloat( 200, 400 );

		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			if ( targetPlayers[ i ]->IsAlive() )
			{
				targetPlayers[ i ]->ApplyAbsVelocityImpulse( slapForce );

				if ( slapDamage > 0 )
				{
					CTakeDamageInfo damageInfo( targetPlayers[ i ], targetPlayers[ i ], slapDamage, DMG_FALL );
					targetPlayers[ i ]->TakeDamage( damageInfo );
				}

				CRecipientFilter filter;
				filter.AddRecipient( targetPlayers[ i ] );
				filter.MakeReliable();

				CBaseEntity::EmitSound( filter, targetPlayers[ i ]->entindex(), "Player.FallDamage" );
			}
		}

		if ( isServerConsole )
		{
			if ( slapDamage > 0 )
			{
				Msg( "Slapped all players in group %s for %d damage.\n", partialName + 1, slapDamage );
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "slapped all players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d damage.\n", partialName + 1, slapDamage ) );
			}
			else
			{
				Msg( "Slapped all players in group %s.\n", partialName + 1 );
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "slapped all players in group " CHAT_DEFAULT "%s.\n", partialName + 1 ) );
			}
		}
		else
		{
			if ( slapDamage > 0 )
			{
				if ( Q_stricmp( partialName, "@me" ) == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slapped themself for " CHAT_DEFAULT "%d damage.\n", pPlayer->GetPlayerName(), slapDamage ) );
				}
				else if ( Q_stricmp( partialName, "@!me" ) == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slapped all players except themself for " CHAT_DEFAULT "%d damage.\n", pPlayer->GetPlayerName(), slapDamage ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slapped all players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d damage.\n", pPlayer->GetPlayerName(), partialName + 1, slapDamage ) );
				}
			}
			else
			{
				if ( Q_stricmp( partialName, "@me" ) == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slapped themself.\n", pPlayer->GetPlayerName() ) );
				}
				else if ( Q_stricmp( partialName, "@!me" ) == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slapped all players except themself.\n", pPlayer->GetPlayerName() ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slapped all players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pPlayer->GetPlayerName(), partialName + 1 ) );
				}
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Slay player
//-----------------------------------------------------------------------------
static void SlayPlayerCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "f" ) )  // f == slay permission
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa slay <name|#userid>\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa slay <name|#userid>\n" );
		}
		return;
	}

	const char* partialName = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	if ( partialName[ 0 ] == '@' )
	{
		if ( Q_stricmp( partialName, "@dead" ) == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "Can't target dead players.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't target dead players.\n" );
			}
			return;
		}
		else if ( isServerConsole && Q_stricmp( partialName, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( partialName, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @!me from the server console\n" );
			return;
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( partialName ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pPlayer ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pPlayer ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( partialName[ 0 ] == '#' )
	{
		int userID = atoi( &partialName[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), partialName ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			if ( targetPlayers[ i ]->IsAlive() )
			{
				targetPlayers[ i ]->CommitSuicide();
			}
		}

		if ( isServerConsole )
		{
			Msg( "Slain players in group %s.\n", partialName + 1 );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "slew players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", partialName + 1 ) );
		}
		else
		{
			if ( Q_stricmp( partialName, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slew themself.\n", pPlayer->GetPlayerName() ) );
			}
			else if ( Q_stricmp( partialName, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slew all players except themself.\n", pPlayer->GetPlayerName() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "slew players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pPlayer->GetPlayerName(), partialName + 1 ) );
			}
		}
	}
	else if ( pTarget )
	{
		if ( !pTarget->IsAlive() )
		{
			if ( isServerConsole )
			{
				Msg( "Can't target dead players.\n", pTarget->GetPlayerName() ); // target dead players
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_RED "Can't target dead players.\n", pTarget->GetPlayerName() ) );
			}
			return;
		}

		pTarget->CommitSuicide();

		if ( isServerConsole )
		{
			Msg( "Slain %s\n", pTarget->GetPlayerName() );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "slew " CHAT_DEFAULT "%s.\n", pTarget->GetPlayerName() ) );
		}
		else
		{
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "%s " CHAT_ADMIN "slew " CHAT_DEFAULT "%s.\n", pPlayer->GetPlayerName(), pTarget->GetPlayerName() ) );
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Kick player
//-----------------------------------------------------------------------------
static void KickPlayerCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "c" ) )  // c == kick
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa kick <name|#userid> [reason]\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa kick <name|#userid> [reason]\n" );
		}
		return;
	}

	const char* partialName = args.Arg( 2 );
	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	CUtlString reason;
	for ( int i = 3; i < args.ArgC(); i++ )
	{
		reason.Append( args.Arg( i ) );
		if ( i < args.ArgC() - 1 )
		{
			reason.Append( " " );
		}
	}

	if ( partialName[ 0 ] == '@' )
	{
		if ( isServerConsole && Q_stricmp( partialName, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( partialName, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( partialName ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pPlayer ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pPlayer ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	else if ( partialName[ 0 ] == '#' )
	{
		int userID = atoi( &partialName[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), partialName ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		pTarget = matchingPlayers[ 0 ];
	}

	if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			if ( reason.Length() > 0 )
			{
				engine->ServerCommand( UTIL_VarArgs( "kickid %d %s\n", pTarget->GetUserID(), reason.Get() ) );
			}
			else
			{
				engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pTarget->GetUserID() ) );
			}
		}

		if ( isServerConsole )
		{
			Msg( "Kicked players in group %s.\n", partialName + 1 );
			UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "kicked players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", partialName + 1 ) );
		}
		else
		{
			if ( Q_stricmp( partialName, "@me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "kicked themself from the server.\n", pPlayer->GetPlayerName() ) );
			}
			else if ( Q_stricmp( partialName, "@!me" ) == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "kicked all players except themself.\n", pPlayer->GetPlayerName() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "kicked players in group " CHAT_DEFAULT "%s" CHAT_ADMIN ".\n", pPlayer->GetPlayerName(), partialName + 1 ) );
			}
		}
	}
	else if ( pTarget )
	{
		const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

		CHL2MP_Admin* pAdmin = CHL2MP_Admin::GetAdmin( targetSteamID );

		if ( pAdmin != nullptr && pAdmin->HasPermission( ADMIN_ROOT ) && ( pTarget != pPlayer ) && !isServerConsole )
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Cannot target this player (root admin privileges).\n" );
			return;
		}

		// Handle individual kicking
		if ( reason.Length() > 0 )
		{
			engine->ServerCommand( UTIL_VarArgs( "kickid %d %s\n", pTarget->GetUserID(), reason.Get() ) );
		}
		else
		{
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pTarget->GetUserID() ) );
		}

		if ( isServerConsole )
		{
			Msg( "Kicked %s (%s)\n", pTarget->GetPlayerName(), reason.Length() > 0 ? reason.Get() : "No reason provided" );

			if ( reason.Length() > 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "kicked " CHAT_DEFAULT "%s " CHAT_SPEC "(%s)\n",
					pTarget->GetPlayerName(), reason.Get() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "kicked " CHAT_DEFAULT "%s\n",
					pTarget->GetPlayerName() ) );
			}
		}
		else
		{
			if ( reason.Length() > 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "kicked " CHAT_DEFAULT "%s " CHAT_SPEC "(%s)\n",
					pPlayer->GetPlayerName(), pTarget->GetPlayerName(), reason.Get() ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "kicked " CHAT_DEFAULT "%s\n",
					pPlayer->GetPlayerName(), pTarget->GetPlayerName() ) );
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: bans a player
//-----------------------------------------------------------------------------
static void BanPlayerCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "d" ) ) // d == ban
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}

	if ( args.ArgC() < 4 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa ban <name|#userid> <time> [reason]\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa ban <name|#userid> <time> [reason]\n" );
		}
		return;
	}

	const char* partialName = args.Arg( 2 );
	const char* timeArg = args.Arg(3);

	// only digits to avoid accidental permabans
	bool isValidTime = true;
	for (int i = 0; i < Q_strlen(timeArg); ++i) 
	{
		if (!isdigit(timeArg[i])) 
		{
			isValidTime = false;
			break;
		}
	}

	if (!isValidTime) 
	{
		if (isServerConsole) 
		{
			Msg("Invalid ban time provided.\n");
		} 
		else 
		{
			UTIL_PrintToClient(pPlayer, CHAT_RED "Invalid ban time provided.\n");
		}
		return;
	}

	int banTime = atoi(timeArg);

	if ( banTime < 0 )
	{
		if ( isServerConsole )
		{
			Msg( "Invalid ban time provided.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid ban time provided.\n" );
		}
		return;
	}

	// we combine all the arguments beyond the time 
	// so that we don't have to use quotes
	CUtlString reason;
	for ( int i = 4; i < args.ArgC(); i++ )
	{
		reason.Append( args.Arg( i ) );
		if ( i < args.ArgC() - 1 )
		{
			reason.Append( " " );
		}
	}

	CUtlVector<CBasePlayer*> targetPlayers;
	CBasePlayer* pTarget = nullptr;

	// special group targets
	if ( partialName[ 0 ] == '@' )
	{
		if ( isServerConsole && Q_stricmp( partialName, "@me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @me from the server console\n" );
			return;
		}
		else if ( isServerConsole && Q_stricmp( partialName, "@!me" ) == 0 )
		{
			if ( isServerConsole )
				Msg( "Can't use @!me from the server console\n" );
			else
				UTIL_PrintToClient( pPlayer, CHAT_RED "Can't use @!me from the server console\n" );
		}

		if ( HL2MPAdmin()->FindSpecialTargetGroup( partialName ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pTarget = UTIL_PlayerByIndex( i );
				if ( !pTarget || !pTarget->IsPlayer() )
				{
					continue;
				}

				if ( HL2MPAdmin()->IsAllPlayers() ||
					( HL2MPAdmin()->IsAllBluePlayers() && pTarget->GetTeamNumber() == TEAM_COMBINE ) ||
					( HL2MPAdmin()->IsAllRedPlayers() && pTarget->GetTeamNumber() == TEAM_REBELS ) ||
					( HL2MPAdmin()->IsAllButMePlayers() && pTarget != pPlayer ) ||
					( HL2MPAdmin()->IsMe() && pTarget == pPlayer ) ||
					( HL2MPAdmin()->IsAllAlivePlayers() && pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllDeadPlayers() && !pTarget->IsAlive() ) ||
					( HL2MPAdmin()->IsAllBotsPlayers() && pTarget->IsBot() ) ||
					( HL2MPAdmin()->IsAllHumanPlayers() && !pTarget->IsBot() ) )
				{
					targetPlayers.AddToTail( pTarget );
				}
			}

			if ( targetPlayers.Count() == 0 )
			{
				if ( isServerConsole )
				{
					Msg( "No players found matching the target group.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching the target group.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid special target specifier.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid special target specifier.\n" );
			}
			return;
		}
	}
	// userID targeting
	else if ( partialName[ 0 ] == '#' )
	{
		int userID = atoi( &partialName[ 1 ] );
		if ( userID > 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
				if ( pLoopPlayer && pLoopPlayer->GetUserID() == userID )
				{
					pTarget = pLoopPlayer;
					break;
				}
			}

			if ( !pTarget )
			{
				if ( isServerConsole )
				{
					Msg( "No player found with that UserID.\n" );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_RED "No player found with that UserID.\n" );
				}
				return;
			}
		}
		else
		{
			if ( isServerConsole )
			{
				Msg( "Invalid UserID provided.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid UserID provided.\n" );
			}
			return;
		}
	}
	else
	{
		// name-based banning, 
		// supports partial names
		CUtlVector<CBasePlayer*> matchingPlayers;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pLoopPlayer = UTIL_PlayerByIndex( i );
			if ( pLoopPlayer && Q_stristr( pLoopPlayer->GetPlayerName(), partialName ) )
			{
				matchingPlayers.AddToTail( pLoopPlayer );
			}
		}

		if ( matchingPlayers.Count() == 0 )
		{
			if ( isServerConsole )
			{
				Msg( "No players found matching that name.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "No players found matching that name.\n" );
			}
			return;
		}
		else if ( matchingPlayers.Count() > 1 )
		{
			if ( isServerConsole )
			{
				Msg( "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					Msg( "%s\n", matchingPlayers[ i ]->GetPlayerName() );
				}
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Multiple players match that partial name:\n" );
				for ( int i = 0; i < matchingPlayers.Count(); i++ )
				{
					UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN_LIGHT "%s\n", matchingPlayers[ i ]->GetPlayerName() ) );
				}
			}
			return;
		}

		// we have exactly one match!
		pTarget = matchingPlayers[ 0 ];
	}

	const static char defaultBanMsg[ 128 ] = "You have been banned from this server";
	const static char defaultPermaBanMsg[ 128 ] = "You have been permanently banned from this server";

	// LOGGING ONLY: not very clean, sorry
	CUtlString banDuration;
	if (banTime == 0) 
	{
		banDuration = "permanently";
	} 
	else 
	{
		banDuration = UTIL_VarArgs("for %d minute%s", banTime, banTime > 1 ? "s" : "");
	}

	// handle banning multiple players (target group)
	if ( targetPlayers.Count() > 0 )
	{
		for ( int i = 0; i < targetPlayers.Count(); i++ )
		{
			CBasePlayer* pTarget = targetPlayers[ i ];
			const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

			// ban hammer time!
			if ( banTime == 0 )
			{
				if ( reason.Length() > 0 )
				{
					engine->ServerCommand( UTIL_VarArgs( "banid 0 %s; kickid %d %s\n", targetSteamID, pTarget->GetUserID(), reason.Get() ) );
				}
				else
				{
					engine->ServerCommand( UTIL_VarArgs( "banid 0 %s; kickid %d %s\n", targetSteamID, pTarget->GetUserID(), defaultPermaBanMsg ) );
				}
				engine->ServerCommand( "writeid\n" );
			}
			else
			{
				// temp ban
				if ( reason.Length() > 0 )
				{
					engine->ServerCommand( UTIL_VarArgs( "banid %d %s; kickid %d %s\n", banTime, targetSteamID, pTarget->GetUserID(), reason.Get() ) );
				}
				else
				{
					engine->ServerCommand( UTIL_VarArgs( "banid %d %s; kickid %d %s\n", banTime, targetSteamID, pTarget->GetUserID(), defaultBanMsg ) );
				}
			}
		}

		if ( isServerConsole )
		{
			if ( banTime == 0 )
			{
				Msg( "Banned players in group %s permanently.\n", partialName + 1 );
			}
			else
			{
				Msg( "Banned players in group %s for %d minute%s.\n", partialName + 1, banTime, banTime > 1 ? "s" : "" );
			}
			if ( banTime == 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "banned players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently.\n", partialName + 1 ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_DEFAULT "Console " CHAT_ADMIN "banned players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s.\n", partialName + 1, banTime, banTime > 1 ? "s" : "" ) );
			}
		}
		else
		{
			if ( Q_stricmp( partialName, "@me" ) == 0 )
			{
				if ( banTime == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned themself permanently.\n", pPlayer->GetPlayerName() ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned themself for " CHAT_DEFAULT "%d minute%s.\n", pPlayer->GetPlayerName(), banTime, banTime > 1 ? "s" : "" ) );
				}
			}
			else if ( Q_stricmp( partialName, "@!me" ) == 0 )
			{
				if ( banTime == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned all players except themself permanently.\n", pPlayer->GetPlayerName() ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned all players except themself for " CHAT_DEFAULT "%d minute%s.\n", pPlayer->GetPlayerName(), banTime, banTime > 1 ? "s" : "" ) );
				}
			}
			else
			{
				if ( banTime == 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently.\n", pPlayer->GetPlayerName(), partialName + 1 ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned players in group " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s.\n", pPlayer->GetPlayerName(), partialName + 1, banTime, banTime > 1 ? "s" : "" ) );
				}
			}
		}
	}
	else if ( pTarget )
	{
		const char* targetSteamID = engine->GetPlayerNetworkIDString( pTarget->edict() );

		CHL2MP_Admin* pAdmin = CHL2MP_Admin::GetAdmin( targetSteamID );

		if ( pAdmin != nullptr && pAdmin->HasPermission( ADMIN_ROOT ) && ( pTarget != pPlayer ) && !isServerConsole )
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Cannot target this player (root admin privileges).\n" );
			return;
		}

		if ( banTime == 0 )
		{
			if ( reason.Length() > 0 )
			{
				engine->ServerCommand( UTIL_VarArgs( "banid 0 %s; kickid %d %s\n", targetSteamID, pTarget->GetUserID(), reason.Get() ) );
			}
			else
			{
				engine->ServerCommand( UTIL_VarArgs( "banid 0 %s; kickid %d %s\n", targetSteamID, pTarget->GetUserID(), defaultPermaBanMsg ) );
			}
			engine->ServerCommand( "writeid\n" );
		}
		else
		{
			if ( reason.Length() > 0 )
			{
				engine->ServerCommand( UTIL_VarArgs( "banid %d %s; kickid %d %s\n", banTime, targetSteamID, pTarget->GetUserID(), reason.Get() ) );
			}
			else
			{
				engine->ServerCommand( UTIL_VarArgs( "banid %d %s; kickid %d %s\n", banTime, targetSteamID, pTarget->GetUserID(), defaultBanMsg ) );
			}
		}

		if ( isServerConsole )
		{
			if ( banTime == 0 )
			{
				Msg( "Banned %s permanently (%s)\n", pTarget->GetPlayerName(), reason.Length() > 0 ? reason.Get() : "No reason provided" );
			}
			else
			{
				Msg( "Banned %s for %d minute%s (%s)\n",
					pTarget->GetPlayerName(), banTime, banTime > 1 ? "s" : "", reason.Length() > 0 ? reason.Get() : "No reason provided" );
			}

			if ( banTime == 0 )
			{
				if ( reason.Length() > 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs(
						CHAT_DEFAULT "Console " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently " CHAT_SPEC "(%s)\n",
						pTarget->GetPlayerName(), reason.Get() ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs(
						CHAT_DEFAULT  "Console " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently.\n",
						pTarget->GetPlayerName() ) );
				}
			}
			else
			{
				if ( reason.Length() > 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs(
						CHAT_DEFAULT "Console " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s. " CHAT_SPEC "(%s)\n",
						pTarget->GetPlayerName(), banTime, banTime > 1 ? "s" : "", reason.Get() ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs(
						CHAT_DEFAULT  "Console " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d " "minute%s.\n",
						pTarget->GetPlayerName(), banTime, banTime > 1 ? "s" : "" ) );
				}
			}
		}
		else
		{
			if ( banTime == 0 )
			{
				if ( reason.Length() > 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently " CHAT_SPEC "(%s).\n",
						pPlayer->GetPlayerName(), pTarget->GetPlayerName(), reason.Get() ) );
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently.\n",
						pPlayer->GetPlayerName(), pTarget->GetPlayerName() ) );
				}
			}
			else
			{
				if ( reason.Length() > 0 )
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s " CHAT_SPEC "(%s).\n",
						pPlayer->GetPlayerName(), pTarget->GetPlayerName(), banTime, banTime > 1 ? "s" : "", reason.Get() ) );

					CHL2MP_Admin::LogAction(
						pPlayer,
						pTarget,
						"banned",
						UTIL_VarArgs("%s (Reason: %s)", banDuration.Get(), reason.Get())
					);
				}
				else
				{
					UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "Admin " CHAT_DEFAULT "%s " CHAT_ADMIN "banned " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s.\n",
						pPlayer->GetPlayerName(), pTarget->GetPlayerName(), banTime, banTime > 1 ? "s" : "" ) );

					CHL2MP_Admin::LogAction(
						pPlayer,
						pTarget,
						"banned",
						banDuration.Get()
					);
				}
			}
		}
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Player not found.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Player not found.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a SteamID3 to the banned list
//-----------------------------------------------------------------------------
static void AddBanCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "m" ) ) // rcon only!!!
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}

	if ( args.ArgC() < 4 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa addban <time> <SteamID3> [reason]\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa addban <time> <SteamID3> [reason]\n" );
		}
		return;
	}

	int banTime = atoi( args.Arg( 2 ) );
	if ( banTime < 0 )
	{
		if ( isServerConsole )
		{
			Msg( "Invalid ban time provided.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid ban time provided.\n" );
		}
		return;
	}

	char steamID[ 64 ];
	Q_snprintf( steamID, sizeof( steamID ), "%s%s%s%s%s", args.Arg( 3 ), args.Arg( 4 ), args.Arg( 5 ), args.Arg( 6 ), args.Arg( 7 ) );

	const char* idPart = Q_strstr( steamID, ":" ) + 3;  // skip [U:1:
	const char* closingBracket = Q_strstr( steamID, "]" );

	if ( !closingBracket || idPart >= closingBracket )
	{
		if ( isServerConsole )
		{
			Msg( "Invalid SteamID format. Missing closing bracket.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid SteamID format. Missing closing bracket.\n" );
		}
		return;
	}

	// ensure the part between the last colon and closing bracket contains only digits
	for ( const char* c = idPart; c < closingBracket; ++c )
	{
		if ( !isdigit( *c ) )
		{
			if ( isServerConsole )
			{
				Msg( "Invalid SteamID format. The part after [U:1: must contain only numbers.\n" );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid SteamID format.\n" CHAT_ADMIN "The part after [U:1: must contain only numbers.\n" );
			}
			return;
		}
	}

	if ( IsSteamIDAdmin( steamID ) )
	{
		if ( isServerConsole )
		{
			Msg( "Cannot ban SteamID %s. This player is an admin.\n", steamID );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "This player is an admin and cannot be banned.\n" );
		}
		return;
	}

	FileHandle_t file = filesystem->Open( "cfg/banned_user.cfg", "r", "MOD" );

	if ( file )
	{
		const int bufferSize = 1024;
		char buffer[ bufferSize ];

		while ( filesystem->ReadLine( buffer, bufferSize, file ) )
		{
			if ( Q_stristr( buffer, steamID ) )
			{
				if ( isServerConsole )
				{
					Msg( "SteamID %s is already banned.\n", steamID );
				}
				else
				{
					UTIL_PrintToClient( pPlayer, CHAT_ADMIN "SteamID is already banned.\n" );
				}
				filesystem->Close( file );
				return;
			}
		}

		filesystem->Close( file );
	}
	else
	{
		if ( isServerConsole )
		{
			Msg( "Failed to open cfg/banned_user.cfg for reading.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Failed to read the ban list.\n" );
		}
		return;
	}

	const char* reason = "";
	if ( args.ArgC() > 8 )
	{
		reason = args.ArgS() + Q_strlen( args.Arg( 0 ) ) + Q_strlen( args.Arg( 1 ) ) + Q_strlen( args.Arg( 2 ) ) + 1;
		reason += Q_strlen( steamID );
	}

	if ( Q_strncmp( steamID, "[U:", 3 ) != 0 || Q_strlen( steamID ) < 6 )
	{
		if ( isServerConsole )
		{
			Msg( "Invalid SteamID format. SteamID must start with [U: and be valid.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid SteamID format.\n" CHAT_ADMIN "SteamID must start with " CHAT_ADMIN_LIGHT "[U:1: \n" );
		}
		return;
	}

	if ( banTime == 0 )
	{
		engine->ServerCommand( UTIL_VarArgs( "banid 0 %s\n", steamID ) );
	}
	else
	{
		engine->ServerCommand( UTIL_VarArgs( "banid %d %s\n", banTime, steamID ) );
	}

	engine->ServerCommand( "writeid\n" );

	if ( isServerConsole )
	{
		Msg( "Added ban for SteamID %s for %d minutes. Reason: %s\n", steamID, banTime, Q_strlen( reason ) > 0 ? reason : "No reason provided" );
	}
	else
	{
		if ( banTime == 0 )
		{
			// permanent ban
			if ( Q_strlen( reason ) > 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "SteamID " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently banned by " CHAT_DEFAULT "%s " CHAT_SPEC "(Reason: %s)\n",
					steamID,
					pPlayer->GetPlayerName(),
					reason ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "SteamID " CHAT_DEFAULT "%s " CHAT_ADMIN "permanently banned by " CHAT_DEFAULT "%s\n",
					steamID,
					pPlayer->GetPlayerName() ) );
			}
		}
		else
		{
			// temporary ban
			if ( Q_strlen( reason ) > 0 )
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "SteamID " CHAT_DEFAULT "%s " CHAT_ADMIN "banned by " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s. " CHAT_SPEC "(Reason: %s)\n",
					steamID,
					pPlayer->GetPlayerName(),
					banTime,
					banTime > 1 ? "s" : "",
					reason ) );
			}
			else
			{
				UTIL_PrintToAllClients( UTIL_VarArgs( CHAT_ADMIN "SteamID " CHAT_DEFAULT "%s " CHAT_ADMIN "banned by " CHAT_DEFAULT "%s " CHAT_ADMIN "for " CHAT_DEFAULT "%d minute%s.\n",
					steamID,
					pPlayer->GetPlayerName(),
					banTime,
					banTime > 1 ? "s" : "" ) );
			}
		}

	}
}

//-----------------------------------------------------------------------------
// Purpose: removes a ban from the banned user list
//-----------------------------------------------------------------------------
static void UnbanPlayerCommand( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	bool isServerConsole = !pPlayer && UTIL_IsCommandIssuedByServerAdmin();

	if ( !pPlayer && !isServerConsole )
	{
		Msg( "Command must be issued by a player or the server console.\n" );
		return;
	}

	if ( pPlayer && !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "e" ) )  // "e" flag for unban permission
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}
	// we need to break down the SteamID into multiple arguments because colons get 
	// interpreted as dividers by Ccommand, which causes the string to stop at the first one
	// check for minimum argument count (expects 8 arguments for a fully split SteamID)
	if ( args.ArgC() < 7 )
	{
		if ( isServerConsole )
		{
			Msg( "Usage: sa unban <SteamID3>\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Usage: " CHAT_ADMIN_LIGHT "sa unban <SteamID3>\n" );
		}
		return;
	}

	// reassemble the SteamID: "[U:1:000000000]"
	char steamID[ 64 ];
	Q_snprintf( steamID, sizeof( steamID ), "%s%s%s%s%s", args.Arg( 2 ), args.Arg( 3 ), args.Arg( 4 ), args.Arg( 5 ), args.Arg( 6 ) );

	// validate the reassembled SteamID
	if ( Q_strncmp( steamID, "[U:", 3 ) != 0 || Q_strlen( steamID ) < 6 )
	{
		if ( isServerConsole )
		{
			Msg( "Invalid SteamID format. SteamID must start with [U: and be valid.\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_RED "Invalid SteamID format.\n" CHAT_ADMIN "SteamID must start with " CHAT_ADMIN_LIGHT "[U:1: \n" );
		}
		return;
	}

	// check if the SteamID exists already
	FileHandle_t file = filesystem->Open( "cfg/banned_user.cfg", "r", "MOD" );

	if ( file )
	{
		const int bufferSize = 1024;
		char buffer[ bufferSize ];
		bool steamIDFound = false;

		while ( filesystem->ReadLine( buffer, bufferSize, file ) )
		{
			if ( Q_stristr( buffer, steamID ) )
			{
				steamIDFound = true;
				break;  // no need to continue reading if we found the SteamID
			}
		}

		filesystem->Close( file );

		if ( steamIDFound )
		{
			engine->ServerCommand( UTIL_VarArgs( "removeid %s\n", steamID ) );
			engine->ServerCommand( "writeid\n" );

			if ( isServerConsole )
			{
				Msg( "SteamID %s has been unbanned.\n", steamID );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "SteamID " CHAT_ADMIN_LIGHT "%s " CHAT_ADMIN "has been unbanned.\n", steamID ) );
			}
		}
		else
		{
			// SteamID was not found in the banned_user.cfg
			if ( isServerConsole )
			{
				Msg( "SteamID %s was not found in the ban list.\n", steamID );
			}
			else
			{
				UTIL_PrintToClient( pPlayer, UTIL_VarArgs( CHAT_ADMIN "SteamID " CHAT_ADMIN_LIGHT "%s " CHAT_ADMIN "was not found in the ban list.\n", steamID ) );
			}
		}
	}
	else
	{
		// if I failed to open the banned_user.cfg file, which
		// shouldn't happen since it's a default file for Source
		if ( isServerConsole )
		{
			Msg( "Failed to open cfg/banned_user.cfg\n" );
		}
		else
		{
			UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Failed to read the ban list.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Show all admin commands via "sa" main command
//-----------------------------------------------------------------------------
static void AdminCommand( const CCommand& args )
{
	if ( !g_bAdminSystem )
	{
		if ( UTIL_IsCommandIssuedByServerAdmin() )
			Msg( "Admin system disabled by the -noadmin launch command\nRemove launch command and restart the server\n" );
		else
		{
			CBasePlayer* pPlayer = UTIL_GetCommandClient();
			if ( pPlayer )
			{
				UTIL_PrintToClient( pPlayer, CHAT_RED "Admin system disabled by the -noadmin launch command\n" );
				return;
			}
		}
		return;
	}

	// get the subcommand
	const char* subCommand = args.Arg( 1 );

	/****************************************/
	// COMMAND ISSUED IN THE RCON CONSOLE   //
	/****************************************/
	if ( UTIL_IsCommandIssuedByServerAdmin() )
	{
		if ( Q_stricmp( subCommand, "say" ) == 0 )
		{
			AdminSay( args );
			return;
		}
		if ( Q_stricmp( subCommand, "csay" ) == 0 )
		{
			AdminCSay( args );
			return;
		}
		if ( Q_stricmp( subCommand, "chat" ) == 0 )
		{
			AdminChat( args );
			return;
		}
		if ( Q_stricmp( subCommand, "psay" ) == 0 )
		{
			AdminPSay( args );
			return;
		}
		if ( Q_stricmp( subCommand, "kick" ) == 0 )
		{
			KickPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "ban" ) == 0 )
		{
			BanPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "addban" ) == 0 )
		{
			AddBanCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "unban" ) == 0 )
		{
			UnbanPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "slay" ) == 0 )
		{
			SlayPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "slap" ) == 0 )
		{
			SlapPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "gag" ) == 0 )
		{
			GagPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "ungag" ) == 0 )
		{
			UnGagPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "mute" ) == 0 )
		{
			MutePlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "unmute" ) == 0 )
		{
			UnMutePlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "team" ) == 0 )
		{
			TeamPlayerCommand( args );
			return;
		}
		// doesn't make sense since you need to aim and 
		// you cannot do that from the server console
		/*else if ( Q_stricmp( subCommand, "bring" ) == 0 )
		{
			BringPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp(subCommand, "goto") == 0 )
		{
			GotoPlayerCommand( args );
			return;
		}*/
		else if ( Q_stricmp( subCommand, "map" ) == 0 )
		{
			MapCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "noclip" ) == 0 )
		{
			NoClipPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "cvar" ) == 0 )
		{
			CVarCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "exec" ) == 0 )
		{
			ExecFileCommand( args );
			return;
		}
		// you already have root privileges
		/*else if ( Q_stricmp( subCommand, "rcon" ) == 0 )
		{
			RconCommand( args );
			return;
		}*/
		else if ( Q_stricmp( subCommand, "reloadadmins" ) == 0 )
		{
			ReloadAdminsCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "help" ) == 0 )
		{
			HelpPlayerCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "credits" ) == 0 )
		{
			CreditsCommand( args );
			return;
		}
		else if ( Q_stricmp( subCommand, "version" ) == 0 )
		{
			VersionCommand( args );
			return;
		}

		Msg( "[Server Admin] Usage: sa <command> [argument]\n" );
		Msg( "===== Root Commands =====\n" );
		Msg( "    say <message> -> Sends an admin formatted message to all players in the chat\n" );
		Msg( "    csay <message> -> Sends a centered message to all players\n" );
		Msg( "    psay <name|#userID> <message> -> Sends a private message to a player\n" );
		Msg( "    chat <message> -> Sends a chat message to connected admins only\n" );
		Msg( "    kick <name|#userID> [reason] -> Kick a player\n" );
		Msg( "    ban <name|#userID> <time> [reason] -> Ban a player\n" );
		Msg( "    addban <time> <SteamID3> [reason] -> Add a manual ban to banned_user.cfg\n" );
		Msg( "    unban <SteamID3> -> Remove a banned SteamID from banned_user.cfg\n" );
		Msg( "    slap <name|#userID> [amount] -> Slap a player with damage if defined\n" );
		Msg( "    slay <name|#userID> -> Slay a player\n" );
		Msg( "    noclip <name|#userID> -> Toggle noclip mode for a player\n" );
		Msg( "    team <name|#userID> <team index> -> Move a player to another team\n" );
		Msg( "    gag <name|#userID> -> Gag a player\n" );
		Msg( "    ungag <name|#userID> -> Ungag a player\n" );
		Msg( "    mute <name|#userID> -> Mute a player\n" );
		Msg( "    unmute <name|#userID> -> Unmute a player\n" );
		Msg( "    map <map name> -> Change the map\n" );
		Msg( "    cvar <cvar name> [new value] -> Modify any cvar's value\n" );
		Msg( "    exec <filename> -> Executes a configuration file\n" );
		Msg( "    reloadadmins -> Refresh the admin cache\n" );
		Msg( "    help -> Provide instructions on how to use the admin interface\n" );
		Msg( "    credits -> Display credits\n" );
		Msg( "    version -> Display version\n\n" );
		return;
	}

	/****************************************/
	// COMMAND ISSUED IN THE PLAYER IN-GAME //
	/****************************************/
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
	{
		return;
	}

	if ( Q_stricmp( subCommand, "credits" ) == 0 )
	{
		CreditsCommand( args );
		return;
	}

	// we need the admin to at least have the "admin" flag
	if ( !CHL2MP_Admin::IsPlayerAdmin( pPlayer, "b" ) )
	{
		UTIL_PrintToClient( pPlayer, CHAT_ADMIN "You do not have access to this command.\n" );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		PrintAdminHelp( pPlayer );
		return;
	}

	if ( Q_stricmp( subCommand, "say" ) == 0 )
	{
		AdminSay( args );
		return;
	}
	if ( Q_stricmp( subCommand, "csay" ) == 0 )
	{
		AdminCSay( args );
		return;
	}
	if ( Q_stricmp( subCommand, "psay" ) == 0 )
	{
		AdminPSay( args );
		return;
	}
	if ( Q_stricmp( subCommand, "chat" ) == 0 )
	{
		AdminChat( args );
		return;
	}
	if ( Q_stricmp( subCommand, "kick" ) == 0 )
	{
		KickPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "ban" ) == 0 )
	{
		BanPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "addban" ) == 0 )
	{
		AddBanCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "unban" ) == 0 )
	{
		UnbanPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "slay" ) == 0 )
	{
		SlayPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "slap" ) == 0 )
	{
		SlapPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "gag" ) == 0 )
	{
		GagPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "ungag" ) == 0 )
	{
		UnGagPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "mute" ) == 0 )
	{
		MutePlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "unmute" ) == 0 )
	{
		UnMutePlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "team" ) == 0 )
	{
		TeamPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "bring" ) == 0 )
	{
		BringPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "goto" ) == 0 )
	{
		GotoPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "map" ) == 0 )
	{
		MapCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "noclip" ) == 0 )
	{
		NoClipPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "cvar" ) == 0 )
	{
		CVarCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "exec" ) == 0 )
	{
		ExecFileCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "rcon" ) == 0 )
	{
		RconCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "reloadadmins" ) == 0 )
	{
		ReloadAdminsCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "help" ) == 0 )
	{
		HelpPlayerCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "credits" ) == 0 )
	{
		CreditsCommand( args );
		return;
	}
	else if ( Q_stricmp( subCommand, "version" ) == 0 )
	{
		VersionCommand( args );
		return;
	}
	else
	{
		// admin typed something wrong
		PrintAdminHelp( pPlayer );
	}
}

ConCommand sa( "sa", AdminCommand, "Admin menu.", FCVAR_NONE );

//-----------------------------------------------------------------------------
// Purpose: Initialize the admin system (parse the file, add admins, register commands)
//-----------------------------------------------------------------------------
void CHL2MP_Admin::InitAdminSystem()
{
	if ( CommandLine()->CheckParm( "-noadmin" ) )
		return;

	g_bAdminSystem = true;
	CHL2MP_Admin::ClearAllAdmins();

	// parse admins from the file
	KeyValues* kv = new KeyValues( "Admins" );

	if ( !kv->LoadFromFile( filesystem, "cfg/admin/admins.txt", "GAME" ) )
	{
		Msg( "Error: Unable to load admins.txt\n" );
		kv->deleteThis();
		return;
	}

	KeyValues* pSubKey = kv->GetFirstSubKey();
	while ( pSubKey )
	{
		const char* steamID = pSubKey->GetName();
		const char* permissions = pSubKey->GetString();

		// debug
		DevMsg( "SteamID: %s, Permissions: %s\n", steamID, permissions );

		// add the admin to the global admin list
		CHL2MP_Admin::AddAdmin( steamID, permissions );

		pSubKey = pSubKey->GetNextKey();
	}

	kv->deleteThis();
	DevMsg( "Admin list loaded from admins.txt.\n" );

	if (!filesystem->IsDirectory("cfg/admin/logs", "GAME"))
	{
		filesystem->CreateDirHierarchy("cfg/admin/logs", "GAME");
	}

	char date[9];
	time_t now = time(0);
	strftime(date, sizeof(date), "%Y%m%d", localtime(&now));

	char logFileName[256];
	Q_snprintf(logFileName, sizeof(logFileName), "cfg/admin/logs/ADMINLOG_%s.txt", date);

	g_AdminLogFile = filesystem->Open(logFileName, "a+", "GAME");
	if (!g_AdminLogFile)
	{
		Msg("Error: Unable to create admin log file.\n");
		g_bAdminSystem = false;
	}
	else
	{
		Msg("Admin log initialized: %s\n", logFileName);
	}
}

void CHL2MP_Admin::CheckChatText( char* p, int bufsize )
{
	if ( !g_bAdminSystem )
		return;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if ( !p || bufsize <= 0 )
		return;

	// support for chat commands
	if ( p[ 0 ] == '!' || p[ 0 ] == '/' )
	{
		if ( Q_strncmp( p, "!say", 4 ) == 0 || Q_strncmp( p, "/say", 4 ) == 0 )
		{
			const char* args = p + 4;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa say %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!csay", 5 ) == 0 || Q_strncmp( p, "/csay", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa csay %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!psay", 5 ) == 0 || Q_strncmp( p, "/psay", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa psay %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!chat", 5 ) == 0 || Q_strncmp( p, "/chat", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa chat %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!ban", 4 ) == 0 || Q_strncmp( p, "/ban", 4 ) == 0 )
		{
			const char* args = p + 4;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa ban %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!kick", 5 ) == 0 || Q_strncmp( p, "/kick", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa kick %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!addban", 7 ) == 0 || Q_strncmp( p, "/addban", 7 ) == 0 )
		{
			const char* args = p + 7;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa addban %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!unban", 6 ) == 0 || Q_strncmp( p, "/unban", 6 ) == 0 )
		{
			const char* args = p + 6;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa unban %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!slay", 5 ) == 0 || Q_strncmp( p, "/slay", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa slay %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!slap", 5 ) == 0 || Q_strncmp( p, "/slap", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa slap %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!cvar", 5 ) == 0 || Q_strncmp( p, "/cvar", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa cvar %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!rcon", 5 ) == 0 || Q_strncmp( p, "/rcon", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa rcon %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!map", 4 ) == 0 || Q_strncmp( p, "/map", 4 ) == 0 )
		{
			const char* args = p + 4;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa map %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!gag", 4 ) == 0 || Q_strncmp( p, "/gag", 4 ) == 0 )
		{
			const char* args = p + 4;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa gag %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!ungag", 6 ) == 0 || Q_strncmp( p, "/ungag", 6 ) == 0 )
		{
			const char* args = p + 6;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa ungag %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!mute", 5 ) == 0 || Q_strncmp( p, "/mute", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa mute %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else 	if ( Q_strncmp( p, "!unmute", 7 ) == 0 || Q_strncmp( p, "/unmute", 7 ) == 0 )
		{
			const char* args = p + 7;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa unmute %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!team", 5 ) == 0 || Q_strncmp( p, "/team", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa team %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!bring", 6 ) == 0 || Q_strncmp( p, "/bring", 6 ) == 0 )
		{
			const char* args = p + 6;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa bring %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!goto", 5 ) == 0 || Q_strncmp( p, "/goto", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa goto %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!noclip", 7 ) == 0 || Q_strncmp( p, "/noclip", 7 ) == 0 )
		{
			const char* args = p + 7;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa noclip %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!exec", 5 ) == 0 || Q_strncmp( p, "/exec", 5 ) == 0 )
		{
			const char* args = p + 5;
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa exec %s", args );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!reloadadmins", 13 ) == 0 || Q_strncmp( p, "/reloadadmins", 13 ) == 0 )
		{
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa reloadadmins" );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
			}
			return;
		}

		else if ( Q_strncmp( p, "!help", 5 ) == 0 || Q_strncmp( p, "/help", 5 ) == 0 )
		{
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa help" );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Check your console for output.\n" );
			}
			return;
		}

		else if ( Q_strncmp( p, "!credits", 8 ) == 0 || Q_strncmp( p, "/credits", 8 ) == 0 )
		{
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa credits" );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Check your console for output.\n" );
			}
			return;
		}

		else if ( Q_strncmp( p, "!version", 8 ) == 0 || Q_strncmp( p, "/version", 8 ) == 0 )
		{
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa version" );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Check your console for output.\n" );
			}
			return;
		}

		else if ( Q_strncmp( p, "!sa", 3 ) == 0 || Q_strncmp( p, "/sa", 3 ) == 0 )
		{
			char consoleCmd[ 256 ];

			// convert the chat message into a console command
			Q_snprintf( consoleCmd, sizeof( consoleCmd ), "sa" );

			if ( pPlayer )
			{
				engine->ClientCommand( pPlayer->edict(), consoleCmd );
				UTIL_PrintToClient( pPlayer, CHAT_ADMIN "Check your console for output.\n" );
			}
			return;
		}
	}
}

void CHL2MP_Admin::LogAction(CBasePlayer* pAdmin, CBasePlayer* pTarget, const char* action, const char* details)
{
	if (g_AdminLogFile == FILESYSTEM_INVALID_HANDLE)
		return;

	time_t now = time(0);
	struct tm* localTime = localtime(&now);
	char dateString[11];
	char timeString[9];
	strftime(dateString, sizeof(dateString), "%Y/%m/%d", localTime);
	strftime(timeString, sizeof(timeString), "%H:%M:%S", localTime);

	const char* mapName = STRING(gpGlobals->mapname);

	const char* adminName = pAdmin ? pAdmin->GetPlayerName() : "Console";
	const char* adminSteamID = pAdmin ? engine->GetPlayerNetworkIDString(pAdmin->edict()) : "Console";
	const char* targetName = pTarget ? pTarget->GetPlayerName() : "";
	const char* targetSteamID = pTarget ? engine->GetPlayerNetworkIDString(pTarget->edict()) : "";

	CUtlString logEntry;
	if (pTarget)
	{
		if (Q_strlen(details) > 0)
		{
			logEntry.Format("[%s] %s @ %s => Admin %s <%s> %s %s <%s> %s\n",
				mapName, dateString, timeString, adminName, adminSteamID,
				action, targetName, targetSteamID, details);
		}
		else
		{
			logEntry.Format("[%s] %s @ %s => Admin %s <%s> %s %s <%s>\n",
				mapName, dateString, timeString, adminName, adminSteamID,
				action, targetName, targetSteamID);
		}
	}
	else
	{
		logEntry.Format("[%s] %s @ %s => Admin %s <%s> %s\n",
			mapName, dateString, timeString, adminName, adminSteamID, action);
	}

	filesystem->FPrintf(g_AdminLogFile, "%s", logEntry.Get());
	filesystem->Flush(g_AdminLogFile);
}
