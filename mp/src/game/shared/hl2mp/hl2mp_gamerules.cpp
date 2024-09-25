//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hl2mp_gamerules.h"
#include "viewport_panel_names.h"
#include "gameeventdefs.h"
#include <KeyValues.h>
#include "ammodef.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#else

#include "eventqueue.h"
#include "player.h"
#include "gamerules.h"
#include "game.h"
#include "items.h"
#include "entitylist.h"
#include "mapentities.h"
#include "in_buttons.h"
#include <ctype.h>
#include "voice_gamemgr.h"
#include "iscorer.h"
#include "hl2mp_player.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "team.h"
#include "voice_gamemgr.h"
#include "hl2mp_gameinterface.h"
#include "hl2mp_cvars.h"
#include <networkstringtable_gamedll.h>
#include "iserver.h"
#include "hl2_player.h"
#include "filesystem.h"
#include "networkstringtabledefs.h"
#include "Sprite.h"

#ifdef DEBUG	
#include "hl2mp_bot_temp.h"
#endif

#define DOWNLOADABLE_FILE_TABLENAME "downloadables"

int g_iPreviousLeaderTeam = TEAM_UNASSIGNED;

static bool g_bCountdownActive = false;
static float g_flCountdownEndTime = 0.0f;
static CBasePlayer* g_pPlayerPausedBy = nullptr;

static bool m_bFirstInitialization = true;
static bool g_bGameOverSounds = false;
bool g_bShouldChangeLevel = false;
extern bool g_bCopsVsFugitive;
extern int g_iCountdown;
bool g_bCopsVsFugitiveGame = false;

static int iOvertimeLimit = 0;

extern void respawn(CBaseEntity* pEdict, bool fCopyCorpse);

extern bool FindInList(const char** pStrings, const char* pToFind);

ConVar sv_hl2mp_weapon_respawn_time("sv_hl2mp_weapon_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_crowbar_respawn_time("sv_crowbar_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_stunstick_respawn_time("sv_stunstick_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_pistol_respawn_time("sv_pistol_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_357_respawn_time("sv_357_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_smg1_respawn_time("sv_smg1_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_ar2_respawn_time("sv_ar2_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_shotgun_respawn_time("sv_shotgun_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_crossbow_respawn_time("sv_crossbow_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_frag_respawn_time("sv_frag_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_rpg_respawn_time("sv_rpg_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_hl2mp_item_respawn_time("sv_hl2mp_item_respawn_time", "30", FCVAR_GAMEDLL | FCVAR_NOTIFY);
ConVar sv_report_client_settings("sv_report_client_settings", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY);

ConVar sv_timeleft_enable("sv_timeleft_enable", "1", 0, "If non-zero,enables time left indication on the HUD.", true, 0.0, true, 1.0);
ConVar sv_timeleft_teamscore("sv_timeleft_teamscore", "1", 0, "If non-zero,enables team scores on the HUD (left Combine, right Rebels)\nMust be enabled to use \"sv_timeleft_color_override\".", true, 0.0, true, 1.0);
ConVar sv_timeleft_color_override("sv_timeleft_color_override", "0", 0, "If non-zero, automatically adjust text color to match the current winning team.", true, 0.0, true, 1.0);
ConVar sv_timeleft_r("sv_timeleft_red", "255", 0, "Red intensity.", true, 0.0, true, 255.0);
ConVar sv_timeleft_g("sv_timeleft_green", "255", 0, "Green intensity.", true, 0.0, true, 255.0);
ConVar sv_timeleft_b("sv_timeleft_blue", "255", 0, "Blue intensity.", true, 0.0, true, 255.0);
ConVar sv_timeleft_channel("sv_timeleft_channel", "0", 0, "Text channel (0-5). Use this if text channels conflict in-game", true, 0.0, true, 5.0); // Channels go from 0 to 5 (6 total channels).
ConVar sv_timeleft_x("sv_timeleft_x", "-1");
ConVar sv_timeleft_y("sv_timeleft_y", "0.01");
ConVar sv_hudtargetid_channel("sv_hudtargetid_channel", "4", 0, "Text channel (0-5). Use this if text channels conflict in-game", true, 0.0, true, 5.0);
ConVar sv_fugitivetimer_channel("sv_fugitivetimer_channel", "2", 0, "Text channel (0-5). Use this if text channels conflict in-game", true, 0.0, true, 5.0);

ConVar mp_noblock("mp_noblock", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY, "If non-zero, disable collisions between players");
ConVar sv_domination_messages("sv_domination_messages", "1", 0, "If non-zero, shows domination messages");
ConVar sv_teamkill_kick("sv_teamkill_kick", "1", 0, "If non-zero, kicks players for excessive team killing");
ConVar sv_teamkill_kick_threshold("sv_teamkill_kick_threshold", "8", 0, "After how many team kills to kick the player");
ConVar sv_teamkill_kick_warning("sv_teamkill_kick_warning", "1", 0, "If non-zero, shows a warning for every team kill");

ConVar sv_equalizer_combine_red("sv_equalizer_combine_red", "0", 0, "Sets the Combine's team red color for equalizer mode");
ConVar sv_equalizer_combine_green("sv_equalizer_combine_green", "255", 0, "Sets the Combine's team green color for equalizer mode");
ConVar sv_equalizer_combine_blue("sv_equalizer_combine_blue", "0", 0, "Sets the Combine's team blue color for equalizer mode");
ConVar sv_equalizer_rebels_red("sv_equalizer_rebels_red", "255", 0, "Sets the Rebels's team red color for equalizer mode");
ConVar sv_equalizer_rebels_green("sv_equalizer_rebels_green", "0", 0, "Sets the Rebels's team green color for equalizer mode");
ConVar sv_equalizer_rebels_blue("sv_equalizer_rebels_blue", "0", 0, "Sets the Rebels's team blue color for equalizer mode");

ConVar sv_overtime("sv_overtime", "1", FCVAR_NOTIFY, "If non-zero, additional time will be added if multiple players have the same number of frags at the top of the scoreboard");
ConVar sv_overtime_limit("sv_overtime_limit", "3", 0, "The number of times the game adds time");
ConVar sv_overtime_time("sv_overtime_time", "1", 0, "The amount of time to add in minutes", true, 1.0, true, 5.0);

extern ConVar sv_custom_sounds;

void sv_equalizer_changed(IConVar* pConVar, const char* pOldString, float flOldValue)
{
	if (!((ConVar*)pConVar)->GetBool())  // If equalizer is disabled
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer)
			{
				// Reset player's render color and effects
				pPlayer->SetRenderColor(255, 255, 255);   // Reset color to white
				pPlayer->SetRenderMode(kRenderNormal);    // Reset to normal render mode

				// Access render properties through networked variables
				pPlayer->m_nRenderFX = kRenderFxNone;     // Reset render FX
				pPlayer->m_nRenderMode = kRenderNormal;   // Reset render mode
			}
		}
	}
}

ConVar sv_equalizer("sv_equalizer", "0", 0, "If non-zero, increase player visibility with bright colors", sv_equalizer_changed);

extern ConVar mp_chattime;
extern ConVar mp_autoteambalance;
extern ConVar sv_lockteams;

extern CBaseEntity* g_pLastCombineSpawn;
extern CBaseEntity* g_pLastRebelSpawn;

#define WEAPON_MAX_DISTANCE_FROM_SPAWN 64

#endif


REGISTER_GAMERULES_CLASS(CHL2MPRules);

BEGIN_NETWORK_TABLE_NOBASE(CHL2MPRules, DT_HL2MPRules)

#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bTeamPlayEnabled)),
#else
SendPropBool(SENDINFO(m_bTeamPlayEnabled)),
#endif

END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS(hl2mp_gamerules, CHL2MPGameRulesProxy);
IMPLEMENT_NETWORKCLASS_ALIASED(HL2MPGameRulesProxy, DT_HL2MPGameRulesProxy)

static HL2MPViewVectors g_HL2MPViewVectors(
	Vector(0, 0, 64),       //VEC_VIEW (m_vView) 

	Vector(-16, -16, 0),	  //VEC_HULL_MIN (m_vHullMin)
	Vector(16, 16, 72),	  //VEC_HULL_MAX (m_vHullMax)

	Vector(-16, -16, 0),	  //VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector(16, 16, 36),	  //VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector(0, 0, 28),		  //VEC_DUCK_VIEW		(m_vDuckView)

	Vector(-10, -10, -10),	  //VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector(10, 10, 10),	  //VEC_OBS_HULL_MAX	(m_vObsHullMax)

	Vector(0, 0, 14),		  //VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)

	Vector(-16, -16, 0),	  //VEC_CROUCH_TRACE_MIN (m_vCrouchTraceMin)
	Vector(16, 16, 60)	  //VEC_CROUCH_TRACE_MAX (m_vCrouchTraceMax)
);

static const char* s_PreserveEnts[] =
{
	"ai_network",
	"ai_hint",
	"hl2mp_gamerules",
	"team_manager",
	"player_manager",
	"env_soundscape",
	"env_soundscape_proxy",
	"env_soundscape_triggerable",
	"env_sun",
	"env_wind",
	"env_fog_controller",
	"func_brush",
	"func_wall",
	"func_buyzone",
	"func_illusionary",
	"infodecal",
	"info_projecteddecal",
	"info_node",
	"info_target",
	"info_node_hint",
	"info_player_deathmatch",
	"info_player_combine",
	"info_player_rebel",
	"info_map_parameters",
	"keyframe_rope",
	"move_rope",
	"info_ladder",
	"player",
	"point_viewcontrol",
	"scene_manager",
	"shadow_control",
	"sky_camera",
	"soundent",
	"trigger_soundscape",
	"viewmodel",
	"predicted_viewmodel",
	"worldspawn",
	"point_devshot_camera",
	"", // END Marker
};



#ifdef CLIENT_DLL
void RecvProxy_HL2MPRules(const RecvProp* pProp, void** pOut, void* pData, int objectID)
{
	CHL2MPRules* pRules = HL2MPRules();
	Assert(pRules);
	*pOut = pRules;
}

BEGIN_RECV_TABLE(CHL2MPGameRulesProxy, DT_HL2MPGameRulesProxy)
RecvPropDataTable("hl2mp_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE(DT_HL2MPRules), RecvProxy_HL2MPRules)
END_RECV_TABLE()
#else
void* SendProxy_HL2MPRules(const SendProp* pProp, const void* pStructBase, const void* pData, CSendProxyRecipients* pRecipients, int objectID)
{
	CHL2MPRules* pRules = HL2MPRules();
	Assert(pRules);
	return pRules;
}

BEGIN_SEND_TABLE(CHL2MPGameRulesProxy, DT_HL2MPGameRulesProxy)
SendPropDataTable("hl2mp_gamerules_data", 0, &REFERENCE_SEND_TABLE(DT_HL2MPRules), SendProxy_HL2MPRules)
END_SEND_TABLE()
#endif

#ifndef CLIENT_DLL

class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayer* pListener, CBasePlayer* pTalker, bool& bProximity)
	{
		return (pListener->GetTeamNumber() == pTalker->GetTeamNumber());
	}
};
CVoiceGameMgrHelper g_VoiceGameMgrHelper;
IVoiceGameMgrHelper* g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

#endif

// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
char* sTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"Combine",
	"Rebels",
};

// Excluded file extensions (so we don't upload unwanted file types)
CUtlVector<const char*> mExcludedUploadExts;

// Example function to add extensions to the list
void CHL2MPRules::InitExcludedExtensions()
{
	mExcludedUploadExts.AddToTail("bz2");
	mExcludedUploadExts.AddToTail("cache");
	mExcludedUploadExts.AddToTail("ztmp");
}

// Checking if an extension is excluded
bool IsExtensionExcluded(const char* ext)
{
	for (int i = 0; i < mExcludedUploadExts.Count(); ++i)
	{
		if (Q_stricmp(mExcludedUploadExts[i], ext) == 0)
		{
			return true;
		}
	}
	return false;
}

#ifndef CLIENT_DLL
void CHL2MPRules::RegisterDownloadableFiles(char* path, FileFindHandle_t findHandle, INetworkStringTable* pDownloadables)
{
	int dirLen = strlen(path);

	// Modify the path to include a wildcard for files (e.g., *.wav)
	char searchPattern[MAX_PATH];
	Q_snprintf(searchPattern, sizeof(searchPattern), "%s*.*", path);

	// Iterate over files in the directory
	for (const char* pNextFileName = filesystem->FindFirstEx(searchPattern, "GAME", &findHandle);
		pNextFileName != NULL; pNextFileName = filesystem->FindNext(findHandle))
	{
		path[dirLen] = '\0';  // Reset path length to directory length

		// Check if it's a directory
		if (filesystem->FindIsDirectory(findHandle))
		{
			if (*pNextFileName != '.')
			{
				// Append the directory to the path
				Q_snprintf(path + dirLen, MAX_PATH - dirLen, "%s%c", pNextFileName, CORRECT_PATH_SEPARATOR);

				// Recursively search in subdirectories
				RegisterDownloadableFiles(path, findHandle, pDownloadables);
#ifdef _DEBUG
				// Debug for directories
				Msg("Entering directory: %s\n", path);
#endif
			}
		}
		else
		{
#ifdef _DEBUG
			// Debug for found files
			Msg("Found file: %s\n", pNextFileName);
#endif
			// Only add files that are not in the excluded list
			const char* extension = Q_GetFileExtension(pNextFileName);
			if (!mExcludedUploadExts.HasElement(extension))
			{
				// Add the file to the downloadable table
				Q_snprintf(path + dirLen, MAX_PATH - dirLen, "%s", pNextFileName);

				Msg("Registering file: %s\n", path);

				if (pDownloadables->AddString(true, path) == INVALID_STRING_INDEX)
				{
					Msg("Failed to register file: %s\n", path);
					break; // Stop if we can't register more files
				}
			}
#ifdef _DEBUG
			else
			{
				Msg("File extension excluded: %s\n", pNextFileName);
			}
#endif
		}
	}

	filesystem->FindClose(findHandle);
}
#endif

CHL2MPRules::CHL2MPRules()
{
#ifndef CLIENT_DLL
	if (m_bFirstInitialization)
	{
		if (sv_custom_sounds.GetBool())
		{
			// Get the downloadables string table
			InitExcludedExtensions();

			// Get the downloadables string table
			INetworkStringTable* pDownloadables = networkstringtable->FindTable(DOWNLOADABLE_FILE_TABLENAME);

			if (pDownloadables)
			{
				// Path to your custom sounds using a char array
				char path[MAX_PATH] = "sound/server_sounds/";
				RegisterDownloadableFiles(path, FILESYSTEM_INVALID_FIND_HANDLE, pDownloadables);
			}
		}
	}

	g_iCountdown = 13;
	g_bCopsVsFugitive = false;
	g_bCopsVsFugitiveGame = false;

	// Create the team managers
	for (int i = 0; i < ARRAYSIZE(sTeamNames); i++)
	{
		CTeam* pTeam = static_cast<CTeam*>(CreateEntityByName("team_manager"));
		pTeam->Init(sTeamNames[i], i);

		g_Teams.AddToTail(pTeam);
	}

	m_bTeamPlayEnabled = teamplay.GetBool();
	m_flIntermissionEndTime = 0.0f;
	m_flGameStartTime = 0;

	m_hRespawnableItemsAndWeapons.RemoveAll();
	m_tmNextPeriodicThink = 0;
	m_flCopsVsFugitive = 0;
	m_flRestartGameTime = 0;
	m_bCompleteReset = false;
	m_bChangelevelDone = false;
	m_bFirstInitialization = false;
	g_bGameOverSounds = false;

	iOvertimeLimit = 0;

	m_flBalanceTeamsTime = 0.0f;

	Precache();

#endif
}

const CViewVectors* CHL2MPRules::GetViewVectors()const
{
	return &g_HL2MPViewVectors;
}

const HL2MPViewVectors* CHL2MPRules::GetHL2MPViewVectors()const
{
	return &g_HL2MPViewVectors;
}

CHL2MPRules::~CHL2MPRules(void)
{
#ifndef CLIENT_DLL
	// Note, don't delete each team since they are in the gEntList and will 
	// automatically be deleted from there, instead.
	g_Teams.Purge();
#endif
}

void CHL2MPRules::CreateStandardEntities(void)
{

#ifndef CLIENT_DLL
	// Create the entity that will send our data to the client.

	BaseClass::CreateStandardEntities();

	g_pLastCombineSpawn = NULL;
	g_pLastRebelSpawn = NULL;

#ifdef DBGFLAG_ASSERT
	CBaseEntity* pEnt =
#endif
		CBaseEntity::Create("hl2mp_gamerules", vec3_origin, vec3_angle);
	Assert(pEnt);
#endif
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHL2MPRules::FlWeaponRespawnTime(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	if (weaponstay.GetInt() > 0)
	{
		// make sure it's only certain weapons
		if (!(pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD))
		{
			return 0;		// weapon respawns almost instantly
		}
	}

	if ((FClassnameIs(pWeapon, "weapon_rpg")))
		return sv_rpg_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_crowbar")))
		return sv_crowbar_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_stunstick")))
		return sv_stunstick_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_pistol")))
		return sv_pistol_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_357")))
		return sv_357_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_smg1")))
		return sv_smg1_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_ar2")))
		return sv_ar2_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_shotgun")))
		return sv_shotgun_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_crossbow")))
		return sv_crossbow_respawn_time.GetFloat();
	else if ((FClassnameIs(pWeapon, "weapon_frag")))
		return sv_frag_respawn_time.GetFloat();
	else
		return sv_hl2mp_weapon_respawn_time.GetFloat();
#endif
	return 0;		// weapon respawns almost instantly
}


bool CHL2MPRules::IsIntermission(void)
{
#ifndef CLIENT_DLL
	return m_flIntermissionEndTime > gpGlobals->curtime;
#endif

	return false;
}

void CHL2MPRules::PlayerKilled(CBasePlayer* pVictim, const CTakeDamageInfo& info)
{
#ifndef CLIENT_DLL
	if (IsIntermission())
		return;
	BaseClass::PlayerKilled(pVictim, info);
#endif
}
#ifdef GAME_DLL
CBaseEntity* FindEntityByName(const char* name)
{
	return gEntList.FindEntityByName(NULL, name);
}
#endif

#ifndef CLIENT_DLL
void UpdateGameRules()
{
	CreateGameRulesObject("CHL2MPRules");
}
#endif

#define MAX_WEAPON_SOUNDS 3

void CHL2MPRules::Think(void)
{
#ifndef CLIENT_DLL

	CGameRules::Think();

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

		if (pPlayer && pPlayer->IsAlive())
		{
			// Limit HUD updates to once per second
			if (gpGlobals->curtime > pPlayer->GetNextHudUpdate())
			{
				Vector vecDir;
				AngleVectors(pPlayer->EyeAngles(), &vecDir);

				Vector vecAbsStart = pPlayer->EyePosition();
				Vector vecAbsEnd = vecAbsStart + (vecDir * 2048); // 512 units for targeting range

				trace_t tr;
				UTIL_TraceLine(vecAbsStart, vecAbsEnd, MASK_ALL, pPlayer, COLLISION_GROUP_NONE, &tr);

				CBasePlayer* pPlayerEntity = dynamic_cast<CBasePlayer*>(tr.m_pEnt);

				if (pPlayerEntity && pPlayerEntity->IsPlayer() && pPlayerEntity->IsAlive())
				{
					char entity[256];

					if (IsTeamplay())
					{
						if (pPlayerEntity->GetTeamNumber() == pPlayer->GetTeamNumber())
						{
							if (pPlayerEntity->ArmorValue())
								Q_snprintf(entity, sizeof(entity), "%s\nHP: %.0i\nAP: %.0i\n", pPlayerEntity->GetPlayerName(), pPlayerEntity->GetHealth(), pPlayerEntity->ArmorValue());
							else
								Q_snprintf(entity, sizeof(entity), "%s\nHP: %.0i\n", pPlayerEntity->GetPlayerName(), pPlayerEntity->GetHealth());
						}
						else
						{
							Q_snprintf(entity, sizeof(entity), "%s", pPlayerEntity->GetPlayerName());
						}
					}
					else
					{
						Q_snprintf(entity, sizeof(entity), "%s", pPlayerEntity->GetPlayerName());
					}

					// HUD message setup
					hudtextparms_s tTextParam;
					tTextParam.x = 0.45;
					tTextParam.y = 0.63;
					tTextParam.effect = 0;
					tTextParam.r1 = 255;
					tTextParam.g1 = 128;
					tTextParam.b1 = 0;
					tTextParam.a1 = 255;
					tTextParam.r2 = 255;
					tTextParam.g2 = 128;
					tTextParam.b2 = 0;
					tTextParam.a2 = 255;
					tTextParam.fadeinTime = 0.008;
					tTextParam.fadeoutTime = 0.008;
					tTextParam.holdTime = 1.0;
					tTextParam.fxTime = 0;
					tTextParam.channel = sv_hudtargetid_channel.GetInt();

					UTIL_HudMessage(pPlayer, tTextParam, entity);
				}
				pPlayer->SetNextHudUpdate(gpGlobals->curtime + 1.0f); // Limit HUD updates to once per second
			}
		}
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer && pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
		{
			pPlayer->RemoveAllItems(true);
		}
	}

	if (g_bCopsVsFugitiveGame)
	{
		int iRebelCount = 0;
		int iCombineCount = 0;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
			CHL2MP_Player* player = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

			if (player && player->IsConnected() && player->IsAlive())
			{
				// Check if the player is a fugitive and if their timer is active
				if (player->IsFugitive() && player->IsTimerActive())
				{
					// Increment the fugitive timer
					player->SetFugitiveTimer(player->GetFugitiveTimer() + gpGlobals->frametime);

					// Optionally, print or show the fugitive timer on the HUD
					// For example, use HUD message code here to display the timer

					if (gpGlobals->curtime > m_tmNextPeriodicThink)
					{
						hudtextparms_t textParams;
						textParams.channel = sv_fugitivetimer_channel.GetInt();  // Use a different channel from the time left timer
						textParams.r1 = 255;
						textParams.g1 = 255;
						textParams.b1 = 0;
						textParams.a1 = 255;
						textParams.x = -1;
						textParams.y = 0.05;  // Slightly lower than the time left timer
						textParams.effect = 0;
						textParams.fadeinTime = 0;
						textParams.fadeoutTime = 0;
						textParams.holdTime = 1.10;
						textParams.fxTime = 0;

						// Format the fugitive timer to display as minutes:seconds
						int minutes = (int)player->GetFugitiveTimer() / 60;
						int seconds = (int)player->GetFugitiveTimer() % 60;
						char fugitiveTimerText[64];
						Q_snprintf(fugitiveTimerText, sizeof(fugitiveTimerText), "Fugitive Time: %02d:%02d", minutes, seconds);

						// Display the fugitive timer on the HUD for this player
						UTIL_HudMessage(player, textParams, fugitiveTimerText);
					}
				}
			}

			if (pPlayer && pPlayer->GetTeamNumber() == TEAM_COMBINE)
			{
				iCombineCount++;
			}
			else if (pPlayer && pPlayer->GetTeamNumber() == TEAM_REBELS)
			{
				iRebelCount++;
			}
		}
		if (iRebelCount == 0 && iCombineCount == 0)
		{
			// Notify all players that the game is canceled
			UTIL_PrintToAllClients(CHAT_CONTEXT "Cops vs Fugitive game has been canceled due to lack of players.");

			// Reset game state
			g_bCopsVsFugitiveGame = false;
			engine->ServerCommand("sv_lockteams 0\n");  // Unlock teams

			return;
		}
	}

	if (g_bCopsVsFugitiveGame)
	{
		engine->ServerCommand("sv_lockteams 1\n");
		engine->ServerCommand("sv_join_spec_on_connect 1\n");
		engine->ServerCommand("sv_overtime 0\n");

		int iRebelCount = 0;
		CBasePlayer* pNewFugitive = nullptr;

		// Loop through all players to find a rebel (fugitive) and count rebels
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_REBELS)
			{
				iRebelCount++;
				pNewFugitive = pPlayer;  // We already have a fugitive
				break;
			}
		}

		// If more than one player is on TEAM_REBELS, move the extras back to TEAM_COMBINE, 
		// this is to keep out any potential bugs where more than one player 
		// could end up on the fugitive team
		if (iRebelCount > 1)
		{
			// Reset rebel count to 1 (keep the last player assigned to the fugitive team)
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

				// Ensure valid player, connected, alive, and on the rebels team (fugitive)
				if (pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_REBELS)
				{
					if (pPlayer != pNewFugitive)
					{
						// Move extra rebels back to TEAM_COMBINE (cops)
						pPlayer->ChangeTeam(TEAM_COMBINE);
						pPlayer->SetFugitiveStatus(false);
						pPlayer->SetTimerActive(false);
					}
				}
			}
		}

		// If no fugitive is found (no players on the rebels team), select a new one from the Combine team
		if (iRebelCount == 0)
		{
			int iCombineCount = 0;
			// CBasePlayer* pPotentialFugitive = nullptr;
			CHL2MP_Player* pPotentialFugitive = nullptr;

			// Loop through all players to find someone on the Combine team to become the fugitive
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				// CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

				if (pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_COMBINE)
				{
					iCombineCount++;

					// Pick a random Combine player to become the new fugitive
					if (!pPotentialFugitive || random->RandomInt(0, iCombineCount - 1) == 0)
					{
						pPotentialFugitive = pPlayer;
					}
				}
			}

			// If we found a new fugitive from the Combine team, switch them to rebels
			if (pPotentialFugitive)
			{
				pPotentialFugitive->ChangeTeam(TEAM_REBELS);
				pPotentialFugitive->SetFugitiveStatus(true);
				pPotentialFugitive->SetTimerActive(true);
				UTIL_PrintToAllClients(CHAT_DEFAULT "%s1 " CHAT_CONTEXT "is the fugitive!", pPotentialFugitive->GetPlayerName());
			}
		}
	}

	if (gpGlobals->curtime > m_flCopsVsFugitive && g_bCopsVsFugitive && !g_bCopsVsFugitiveGame)
	{
		hudtextparms_t textParams;
		textParams.channel = sv_timeleft_channel.GetInt();
		textParams.r1 = 0;
		textParams.g1 = 255;
		textParams.b1 = 0;
		textParams.a1 = 255;
		textParams.x = -1;
		textParams.y = 0.01;
		textParams.effect = 0;
		textParams.fadeinTime = 0;
		textParams.fadeoutTime = 0;
		textParams.holdTime = 1.10;
		textParams.fxTime = 0;

		char sCopsVsGFugitive[32];

		g_iCountdown--;
		if (g_iCountdown > 0)
			Q_snprintf(sCopsVsGFugitive, sizeof(sCopsVsGFugitive), "COPS VS FUGITIVE MATCH\n%d", g_iCountdown);
		else
		{
			RestartGame();
			Q_snprintf(sCopsVsGFugitive, sizeof(sCopsVsGFugitive), "GO!");
			engine->ServerCommand("mp_timelimit 20\n");

			CUtlVector<CHL2MP_Player*> nonSpectatorPlayers;

			for (int i = 0; i < MAX_PLAYERS; i++)
			{
				//CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

				if (!pPlayer)
					continue;

				pPlayer->RemoveFlag(FL_FROZEN);
				pPlayer->GiveNamedItem("weapon_crowbar");
				pPlayer->GiveNamedItem("weapon_physcannon");
				pPlayer->GiveNamedItem("weapon_pistol");
				// pPlayer->GiveAmmo(255, "Pistol");

				if (pPlayer->GetTeamNumber() != TEAM_SPECTATOR)
				{
					nonSpectatorPlayers.AddToTail(pPlayer);
				}
			}

			// Pick a random fugitive
			if (nonSpectatorPlayers.Count() > 0)
			{
				int fugitiveIndex = random->RandomInt(0, nonSpectatorPlayers.Count() - 1);
				//CBasePlayer* pFugitive = nonSpectatorPlayers[fugitiveIndex];
				CHL2MP_Player* pFugitive = nonSpectatorPlayers[fugitiveIndex];

				// Set the random player as the fugitive
				pFugitive->ChangeTeam(TEAM_REBELS); 
				pFugitive->SetFugitiveStatus(true);
				pFugitive->SetTimerActive(true);
				Msg("Fugitive set\n");

				ClientPrint(pFugitive, HUD_PRINTCENTER, "YOU ARE THE FUGITIVE!");
				UTIL_PrintToAllClients(CHAT_DEFAULT "%s1 " CHAT_CONTEXT "is the fugitive!", pFugitive->GetPlayerName());

				// Set all other players as cops
				for (int i = 0; i < nonSpectatorPlayers.Count(); i++)
				{
					CHL2MP_Player* pPlayer = nonSpectatorPlayers[i];
					if (pPlayer != pFugitive)
					{
						pPlayer->ChangeTeam(TEAM_COMBINE);
						pPlayer->SetFugitiveStatus(false);
						pPlayer->SetTimerActive(false);
						Msg("Fugitive unset\n");
					}
				}
			}

			g_bCopsVsFugitive = false;
			g_bCopsVsFugitiveGame = true;
			engine->ServerCommand("sv_join_spec_on_connect 1\n");
		}

		
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
			if (pPlayer)
			{
				if (g_iCountdown == 5)
					engine->ClientCommand(pPlayer->edict(), "play server_sounds/five.wav\n");
				else if (g_iCountdown == 4)
					engine->ClientCommand(pPlayer->edict(), "play server_sounds/four.wav\n");
				else if (g_iCountdown == 3)
					engine->ClientCommand(pPlayer->edict(), "play server_sounds/three.wav\n");
				else if (g_iCountdown == 2)
					engine->ClientCommand(pPlayer->edict(), "play server_sounds/two.wav\n");
				else if (g_iCountdown == 1)
					engine->ClientCommand(pPlayer->edict(), "play server_sounds/one.wav\n");
				else if (g_iCountdown == 0)
				{
					int iRandomGo = random->RandomInt(1, 3);

					if (iRandomGo == 1)
						engine->ClientCommand(pPlayer->edict(), "play server_sounds/go1.wav\n");
					else if (iRandomGo == 2)
						engine->ClientCommand(pPlayer->edict(), "play server_sounds/go2.wav\n");
					else if (iRandomGo == 3)
						engine->ClientCommand(pPlayer->edict(), "play server_sounds/go3.wav\n");
				}
			}
		}

		if (g_iCountdown <= 10)
			UTIL_HudMessage(UTIL_GetLocalPlayer(), textParams, sCopsVsGFugitive);

		m_flCopsVsFugitive = gpGlobals->curtime + 1.0f;
	}
	
	// Fix players spectating a spectator if they were in first person view
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		if (pPlayer->IsObserver())
		{
			CBasePlayer* pObservedPlayer = ToBasePlayer(pPlayer->GetObserverTarget());

			if (pObservedPlayer)
			{
				if (pObservedPlayer->GetTeamNumber() == TEAM_SPECTATOR)
				{
					pPlayer->SetObserverMode(OBS_MODE_ROAMING);
				}
			}
		}
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (gpGlobals->curtime > m_tmNextPeriodicThink)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer && !pPlayer->IsBot() && !pPlayer->IsHLTV())
			{
				// Fetch client-side settings from the player
				const char* cl_updaterate = engine->GetClientConVarValue(pPlayer->entindex(), "cl_updaterate");
				const char* cl_cmdrate = engine->GetClientConVarValue(pPlayer->entindex(), "cl_cmdrate");

				// Convert to integer for validation (handle potential NULL or empty strings)
				int updaterate = cl_updaterate && cl_updaterate[0] ? atoi(cl_updaterate) : 0;
				int cmdrate = cl_cmdrate && cl_cmdrate[0] ? atoi(cl_cmdrate) : 0;

				// Log the converted integer values

				bool shouldKick = false;
				char kickReason[128] = "";

				// Check if cl_updaterate is set to 0 or less
				if (updaterate <= 0)
				{
					shouldKick = true;
					Q_snprintf(kickReason, sizeof(kickReason), "cl_updaterate is invalid (value: %d)", updaterate);
				}

				// Check if cl_cmdrate is set to 0 or less
				if (cmdrate <= 0)
				{
					shouldKick = true;
					Q_snprintf(kickReason, sizeof(kickReason), "cl_cmdrate is invalid (value: %d)", cmdrate);
				}

				if (shouldKick)
				{
					// Get the player's user ID instead of the entity index
					int userID = pPlayer->GetUserID();  // This will provide the correct user ID for kicking

					engine->ServerCommand(UTIL_VarArgs("kickid %d %s\n", userID, kickReason));  // Use userID instead of entindex()
					return; // Prevent further processing
				}
			}
		}
	}

	if (teamplay.GetBool() && !IsTeamplay() || !teamplay.GetBool() && IsTeamplay())
	{
		// loop through all players
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->IsConnected() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR && !pPlayer->IsHLTV())
			{
				if (teamplay.GetInt() == 0 && g_pGameRules->IsTeamplay() == 1)
				{
					pPlayer->ChangeTeam(0);
				}
				else
				{				
					sv_timeleft_color_override.SetValue(1);
					pPlayer->ChangeTeam(random->RandomInt(2, 3));
				}
			}
		}
		UpdateGameRules();
		RestartGame();
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer)
		{
			if (mp_noblock.GetBool())
			{
				pPlayer->SetCollisionGroup(COLLISION_GROUP_DEBRIS_TRIGGER);
			}
			else
				pPlayer->SetCollisionGroup(COLLISION_GROUP_PLAYER);
		}
	}

	if (sv_equalizer.GetBool())
	{
		// We're not reinventing the wheel, we'll just use what SF has already done, 
		// find the info_target entity named "sf_equalizer_hax"
		CBaseEntity* pLightingTarget = FindEntityByName("sf_equalizer_hax");

		if (pLightingTarget)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

				if (pPlayer && pPlayer->GetTeamNumber() != TEAM_SPECTATOR)
				{
					// Set render color based on team
					if (pPlayer->GetTeamNumber() == TEAM_COMBINE)
					{
						pPlayer->SetRenderColor(sv_equalizer_combine_red.GetInt(),
							sv_equalizer_combine_green.GetInt(),
							sv_equalizer_combine_blue.GetInt());
					}
					else if (pPlayer->GetTeamNumber() == TEAM_REBELS)
					{
						pPlayer->SetRenderColor(sv_equalizer_rebels_red.GetInt(),
							sv_equalizer_rebels_green.GetInt(),
							sv_equalizer_rebels_blue.GetInt());
					}
					else if (pPlayer->GetTeamNumber() == TEAM_UNASSIGNED)
					{
						const char* szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(pPlayer->edict()), "cl_playermodel");

						if (Q_stristr(szModelName, "models/human"))
						{
							pPlayer->SetRenderColor(sv_equalizer_rebels_red.GetInt(),
								sv_equalizer_rebels_green.GetInt(),
								sv_equalizer_rebels_blue.GetInt());
						}
						else
						{
							pPlayer->SetRenderColor(sv_equalizer_combine_red.GetInt(),
								sv_equalizer_combine_green.GetInt(),
								sv_equalizer_combine_blue.GetInt());
						}
					}

					// Apply render mode and glowing effect for better visibility
					pPlayer->SetRenderMode(kRenderTransAdd);  // Additive blending mode for bright glow

					// Access render properties through networked variables
					pPlayer->m_nRenderFX = kRenderFxGlowShell; // Add glow effect around the player
					pPlayer->SetRenderColorA(255);  // Full brightness

					// Set the lighting origin using the info_target entity handle
					pPlayer->SetLightingOrigin(pLightingTarget);
				}
			}
		}
		else
		{
#ifdef DEBUG
			Warning("Could not find info_target entity named 'sf_equalizer_hax'.\n");
#endif
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

				if (pPlayer && pPlayer->GetTeamNumber() != TEAM_SPECTATOR)
				{
					// Set render color based on team
					if (pPlayer->GetTeamNumber() == TEAM_COMBINE)
					{
						pPlayer->SetRenderColor(sv_equalizer_combine_red.GetInt(),
							sv_equalizer_combine_green.GetInt(),
							sv_equalizer_combine_blue.GetInt());
					}
					else if (pPlayer->GetTeamNumber() == TEAM_REBELS)
					{
						pPlayer->SetRenderColor(sv_equalizer_rebels_red.GetInt(),
							sv_equalizer_rebels_green.GetInt(),
							sv_equalizer_rebels_blue.GetInt());
					}

					else if (pPlayer->GetTeamNumber() == TEAM_UNASSIGNED)
					{
						const char* szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(pPlayer->edict()), "cl_playermodel");

						if (Q_stristr(szModelName, "models/human"))
						{
							pPlayer->SetRenderColor(sv_equalizer_rebels_red.GetInt(),
								sv_equalizer_rebels_green.GetInt(),
								sv_equalizer_rebels_blue.GetInt());
						}
						else
						{
							pPlayer->SetRenderColor(sv_equalizer_combine_red.GetInt(),
								sv_equalizer_combine_green.GetInt(),
								sv_equalizer_combine_blue.GetInt());
						}
					}

					// Apply render mode and glowing effect for better visibility
					pPlayer->SetRenderMode(kRenderTransAdd);  // Additive blending mode for bright glow

					// Access render properties through networked variables
					pPlayer->m_nRenderFX = kRenderFxGlowShell; // Add glow effect around the player
					pPlayer->SetRenderColorA(255);  // Full brightness

					// Set the lighting origin using the info_target entity handle
					pPlayer->SetLightingOrigin(pLightingTarget);
				}
			}
		}
	}

	iConnected = NULL;

	// For match servers
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer)
			iConnected++;
	}

	if (iConnected < 2 && !g_bCopsVsFugitiveGame && !g_bCopsVsFugitive)
	{
		sv_lockteams.SetValue(0);

		if (engine->IsPaused())
			engine->GetIServer()->SetPaused(false);
	}

	// Refactor the team auto balance to actually do something
	if (mp_autoteambalance.GetBool() && IsTeamplay() && !g_bCopsVsFugitive && !g_bCopsVsFugitiveGame)
	{
		if (gpGlobals->curtime > m_flBalanceTeamsTime)
		{
			int iCombine = 0;
			int iRebels = 0;

			int combineScore = 0;  // Initialize Combine team score
			int rebelScore = 0;    // Initialize Rebel team score

			CUtlVector<CBasePlayer*> combinePlayers;
			CUtlVector<CBasePlayer*> rebelPlayers;

			CTeam* pCombine = g_Teams[TEAM_COMBINE];
			CTeam* pRebels = g_Teams[TEAM_REBELS];

			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

				// Only balance teams if at least 2 players are connected
				if (pPlayer)
				{
					if (pPlayer->GetTeamNumber() == TEAM_COMBINE)
					{
						iCombine++;
						combinePlayers.AddToTail(pPlayer); // Track Combine players
						combineScore += pPlayer->FragCount(); // Add player frags to Combine score
					}
					else if (pPlayer->GetTeamNumber() == TEAM_REBELS)
					{
						iRebels++;
						rebelPlayers.AddToTail(pPlayer); // Track Rebel players
						rebelScore += pPlayer->FragCount(); // Add player frags to Rebel score
					}
				}
			}

#ifdef _DEBUG
			Msg("%d Combine players, %d Rebel players, currently with %d connected players.\n", iCombine, iRebels, gpGlobals->maxClients);
			Msg("Current team scores: Combine: %d, Rebels: %d\n", combineScore, rebelScore);
#endif

			// If the difference is 2 or more, the teams are deemed unbalanced
			int difference = abs(iCombine - iRebels);
			if (difference > 1)
			{
				// Determine which team has more players and needs balancing
				CUtlVector<CBasePlayer*>* teamWithMorePlayers = nullptr;
				int teamWithFewerPlayers = 0;
				if (iCombine > iRebels)
				{
					teamWithMorePlayers = &combinePlayers;
					teamWithFewerPlayers = TEAM_REBELS;
#ifdef _DEBUG
					Msg("Unbalanced teams! More Combine players than Rebel players!\n");
#endif
			}
				else if (iRebels > iCombine)
				{
					teamWithMorePlayers = &rebelPlayers;
					teamWithFewerPlayers = TEAM_COMBINE;
#ifdef _DEBUG
					Msg("Unbalanced teams! More Rebel players than Combine players!\n");
#endif
				}

				// Move random players to the other team
				if (teamWithMorePlayers)
				{
					for (int j = 0; j < difference / 2; j++)
					{
						// Select a random player from the team with more players
						int randomIndex = RandomInt(0, teamWithMorePlayers->Count() - 1);
						CBasePlayer* pSelectedPlayer = teamWithMorePlayers->Element(randomIndex);

						if (pSelectedPlayer)
						{
							pSelectedPlayer->ChangeTeam(teamWithFewerPlayers);
							teamWithMorePlayers->Remove(randomIndex); // Remove player after switching

							// Update the scores after switching players
							if (teamWithFewerPlayers == TEAM_REBELS)
							{
								// Deduct from Combine, add to Rebels
								combineScore -= pSelectedPlayer->FragCount();
								rebelScore += pSelectedPlayer->FragCount();
							}
							else if (teamWithFewerPlayers == TEAM_COMBINE)
							{
								// Deduct from Rebels, add to Combine
								rebelScore -= pSelectedPlayer->FragCount();
								combineScore += pSelectedPlayer->FragCount();
							}

#ifdef _DEBUG
							Msg("Switched player %s to the opposite team.\n", pSelectedPlayer->GetPlayerName());
							Msg("Updated team scores: Combine: %d, Rebels: %d\n", combineScore, rebelScore);
#endif
						}
					}
					UTIL_PrintToAllClients(CHAT_INFO "Teams have been balanced!");
					UTIL_ClientPrintAll(HUD_PRINTCONSOLE, "Teams have been balanced!");
				}
			}
			else
			{
#ifdef _DEBUG
				Msg("Teams are balanced!\n");
#endif
			}

			// Update global team scores with recalculated values
			pCombine->SetScore(combineScore);
			pRebels->SetScore(rebelScore);
#ifdef _DEBUG
			Msg("Final team scores after balance: Combine: %d, Rebels: %d\n", combineScore, rebelScore);
#endif
		}
	}
#ifdef _DEBUG
	else if (gpGlobals->curtime > m_flBalanceTeamsTime)
	{
		Msg("Couldn't check team balance! Teamplay is disabled!\n");
	}
#endif

	if (g_fGameOver)   // someone else quit the game already
	{
		if (!g_bGameOverSounds && sv_custom_sounds.GetBool() && !g_bCopsVsFugitiveGame)
		{
			g_bGameOverSounds = true; // Ensure sounds are only played once

			// Count connected players
			int connectedPlayers = 0;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				if (pPlayer && pPlayer->IsConnected())
				{
					connectedPlayers++;
				}
			}

			// Skip the rest if only one player is connected
			if (connectedPlayers <= 1)
			{
				return;
			}

			hudtextparms_t textParams;
			textParams.x = -1;
			textParams.y = 0.05; // Display the text slightly below the top of the screen
			textParams.effect = 0;
			textParams.fadeinTime = 0;
			textParams.fadeoutTime = 1.5;
			textParams.holdTime = mp_chattime.GetInt();
			textParams.fxTime = 0;

			if (IsTeamplay())
			{
				CTeam* pCombine = g_Teams[TEAM_COMBINE];
				CTeam* pRebels = g_Teams[TEAM_REBELS];

				// Check team scores
				if (pCombine->GetScore() > pRebels->GetScore())
				{
					// Blue team wins
					UTIL_PrintToAllClients(CHAT_CONTEXT "Team " CHAT_BLUE "Combine " CHAT_CONTEXT "wins!");
					
					textParams.r1 = 159;
					textParams.g1 = 202;
					textParams.b1 = 242;
					UTIL_HudMessage(UTIL_GetLocalPlayer(), textParams, "TEAM COMBINE WINS!");

					for (int i = 0; i < gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
						if (pPlayer)
						{
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/blue_wins.wav\n");
						}
					}
				}
				else if (pRebels->GetScore() > pCombine->GetScore())
				{
					// Red team wins
					UTIL_PrintToAllClients(CHAT_CONTEXT "Team " CHAT_RED "Rebels " CHAT_CONTEXT "wins!");
					
					textParams.r1 = 255;
					textParams.g1 = 50;
					textParams.b1 = 50;
					UTIL_HudMessage(UTIL_GetLocalPlayer(), textParams, "TEAM REBELS WIN!");
					
					for (int i = 0; i < gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
						if (pPlayer)
						{
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/red_wins.wav\n");
						}
					}
				}
				else
				{
					// It's a draw
					UTIL_PrintToAllClients(CHAT_CONTEXT "Game is a draw!");

					textParams.r1 = 255;
					textParams.g1 = 255;
					textParams.b1 = 255;
					UTIL_HudMessage(UTIL_GetLocalPlayer(), textParams, "GAME DRAW!");

					for (int i = 0; i < gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
						if (pPlayer)
						{
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/draw.wav\n");
						}
					}
				}
			}
			else
			{
				// Identify the top frag count and whether there's a tie
				int highestFrags = -1;
				int tieCount = 0;
				CBasePlayer* pWinner = nullptr;

				// Loop through all players to find the highest frag count and check for ties
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

					if (pPlayer && pPlayer->IsConnected())
					{
						int frags = pPlayer->FragCount();

						// If we find a higher frag count, reset the tie count and update the winner
						if (frags > highestFrags)
						{
							highestFrags = frags;
							tieCount = 1;
							pWinner = pPlayer; // Set the potential winner
						}
						// If another player has the same frag count, increase the tie count
						else if (frags == highestFrags)
						{
							tieCount++;
						}
					}
				}

				// Play appropriate sound based on the results
				for (int i = 0; i < gpGlobals->maxClients; i++)
				{
					CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
					if (pPlayer && pPlayer->IsConnected())
					{
						if (tieCount > 1)
						{
							// Loop through all players and play the tie sound
							for (int i = 0; i < gpGlobals->maxClients; i++)
							{
								CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
								if (pPlayer && pPlayer->IsConnected())
								{
									engine->ClientCommand(pPlayer->edict(), "play server_sounds/tie.wav\n");
								}
							}
						}

						else if (pPlayer == pWinner)
						{
							// Play the "you win" sound for the winner
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/youwin.wav\n");
							UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_DEFAULT "%s " CHAT_CONTEXT "wins!", pPlayer->GetPlayerName()));

							textParams.r1 = 255;
							textParams.g1 = 165;
							textParams.b1 = 0;
							UTIL_HudMessage(UTIL_GetLocalPlayer(), textParams, UTIL_VarArgs("%s WINS!", pPlayer->GetPlayerName()));
						}
						else
						{
							// Play the "game over" sound for everyone else
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/gameover.wav\n");
						}
					}
				}
			}
		}

		g_bCopsVsFugitive = false;
		g_bCopsVsFugitiveGame = false;
		engine->ServerCommand("sv_lockteams 0\n");

		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->curtime)
		{
			if (!m_bChangelevelDone)
			{
				ChangeLevel(); // intermission is over
				m_bChangelevelDone = true;
			}
		}

		return;
	}

	//	float flTimeLimit = mp_timelimit.GetFloat() * 60;
	float flFragLimit = fraglimit.GetFloat();

	if (GetMapRemainingTime() > 0)
	{
		if (sv_timeleft_enable.GetBool())
		{
			if (gpGlobals->curtime > m_tmNextPeriodicThink)
			{
				hudtextparms_t textParams;
				textParams.channel = sv_timeleft_channel.GetInt();
				if (!sv_timeleft_color_override.GetBool())
				{
					textParams.r1 = sv_timeleft_r.GetInt();
					textParams.g1 = sv_timeleft_g.GetInt();
					textParams.b1 = sv_timeleft_b.GetInt();
				}
				else if (g_bCopsVsFugitiveGame)
				{
					textParams.r1 = 255;
					textParams.g1 = 255;
					textParams.b1 = 255;
				}
				textParams.a1 = 255;
				textParams.x = sv_timeleft_x.GetFloat();
				textParams.y = sv_timeleft_y.GetFloat();
				textParams.effect = 0;
				textParams.fadeinTime = 0;
				textParams.fadeoutTime = 0;
				textParams.holdTime = 1.10;
				textParams.fxTime = 0;

				if (!sv_timeleft_teamscore.GetBool() || teamplay.GetInt() < 1)
					sv_timeleft_color_override.SetValue(0);

				int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();

				int iDays, iHours, iMinutes, iSeconds;
				iMinutes = (iTimeRemaining / 60) % 60;
				iSeconds = iTimeRemaining % 60;
				iHours = (iTimeRemaining / 3600) % 24;
				// Yes, this is ridiculous
				iDays = (iTimeRemaining / 86400);

				char stime[64];

				if (IsTeamplay() && sv_timeleft_teamscore.GetBool())
				{
					CTeam* pCombine = g_Teams[TEAM_COMBINE];
					CTeam* pRebels = g_Teams[TEAM_REBELS];

					if ((pCombine->GetScore() > pRebels->GetScore()) && sv_timeleft_color_override.GetBool() && !g_bCopsVsFugitiveGame)
					{
						textParams.r1 = 159;
						textParams.g1 = 202;
						textParams.b1 = 242;
					}
					else if ((pRebels->GetScore() > pCombine->GetScore()) && sv_timeleft_color_override.GetBool() && !g_bCopsVsFugitiveGame)
					{
						textParams.r1 = 255;
						textParams.g1 = 50;
						textParams.b1 = 50;
					}
					else if ((pRebels->GetScore() == pCombine->GetScore()) && sv_timeleft_color_override.GetBool() && !g_bCopsVsFugitiveGame)
					{
						textParams.r1 = 255;
						textParams.g1 = 255;
						textParams.b1 = 255;
					}

					if (iTimeRemaining >= 86400)
						Q_snprintf(stime, sizeof(stime), "%d %2.2d:%2.2d:%2.2d:%2.2d %d ", pCombine->GetScore(), iDays, iHours, iMinutes, iSeconds, pRebels->GetScore());
					else if (iTimeRemaining >= 3600)
						Q_snprintf(stime, sizeof(stime), "%d %2.2d:%2.2d:%2.2d %d ", pRebels->GetScore(), iHours, iMinutes, iSeconds, pRebels->GetScore());
					else
						Q_snprintf(stime, sizeof(stime), "%d %d:%2.2d %d", pCombine->GetScore(), iMinutes, iSeconds, pRebels->GetScore());
				}
				else
					if (iTimeRemaining >= 86400)
						Q_snprintf(stime, sizeof(stime), "%2.2d:%2.2d:%2.2d:%2.2d", iDays, iHours, iMinutes, iSeconds);
					else if (iTimeRemaining >= 3600)
						Q_snprintf(stime, sizeof(stime), "%2.2d:%2.2d:%2.2d", iHours, iMinutes, iSeconds);
					else
						Q_snprintf(stime, sizeof(stime), "%d:%2.2d", iMinutes, iSeconds);


				UTIL_HudMessage(UTIL_GetLocalPlayer(), textParams, stime);

				if (!IsTeamplay())
				{
					// Get the unassigned team
					CTeam* pTeamUnassigned = g_Teams[TEAM_UNASSIGNED];

					if (pTeamUnassigned)
					{
						// Collect all players in the unassigned team
						CUtlVector<CBaseMultiplayerPlayer*> unassignedPlayers;
						for (int i = 1; i <= gpGlobals->maxClients; i++) // Iterate through all players
						{
							CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
							CBaseMultiplayerPlayer* pMultiplayerPlayer = ToBaseMultiplayerPlayer(pPlayer);

							// Check if player is valid, on the unassigned team, and not a spectator
							if (pMultiplayerPlayer && pMultiplayerPlayer->GetTeamNumber() == TEAM_UNASSIGNED && !pMultiplayerPlayer->IsObserver())
							{
								unassignedPlayers.AddToTail(pMultiplayerPlayer);
							}
						}

						// Sort the unassigned players by frags in descending order
						unassignedPlayers.Sort([](CBaseMultiplayerPlayer* const* a, CBaseMultiplayerPlayer* const* b) {
							return (*b)->FragCount() - (*a)->FragCount();
							});

						// Now we loop over each player and display their stats on their own HUD
						for (int i = 0; i < unassignedPlayers.Count(); i++)
						{
							CBaseMultiplayerPlayer* pCurrentPlayer = unassignedPlayers[i];
							int playerRank = i + 1; // Rank starts from 1, not 0

							hudtextparms_t playerTextParams = textParams;
							playerTextParams.y = sv_timeleft_y.GetFloat() + 0.03f; // Position below the time
							playerTextParams.channel = 1;

							// Format the message
							char playerStatText[128];
							Q_snprintf(playerStatText, sizeof(playerStatText), "%d/%d | %d Frag%s", playerRank, unassignedPlayers.Count(), pCurrentPlayer->FragCount(), pCurrentPlayer->FragCount() < 2 ? "" : "s");

							// Display the message to the player
							UTIL_HudMessage(pCurrentPlayer, playerTextParams, playerStatText);
						}
					}
				}
			}
		}
	}	

	if (g_bShouldChangeLevel && GetMapRemainingTime() < 0)
	{
		g_bShouldChangeLevel = false;
		ChangeLevel();
	}

	if (GetMapRemainingTime() < 0)
	{
		CHL2MP_Player* pWinner = nullptr;
		float longestFugitiveTime = 0.0f;

		// Loop through all players and find the fugitive with the longest time
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

			if (pPlayer && pPlayer->IsConnected())
			{
				if (pPlayer->GetFugitiveTimer() > longestFugitiveTime)
				{
					longestFugitiveTime = pPlayer->GetFugitiveTimer();
					pWinner = pPlayer;
				}
			}
		}

		// If we found a winner, declare them
		if (pWinner)
		{
			int minutes = static_cast<int>(longestFugitiveTime) / 60;
			int seconds = static_cast<int>(longestFugitiveTime) % 60;

			UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_DEFAULT "%s " CHAT_CONTEXT "is the winner with a fugitive time of " CHAT_DEFAULT "%02d:%02d!", pWinner->GetPlayerName(), minutes, seconds));

			// End the game or perform additional actions (reset, reward, etc.)
			g_bCopsVsFugitiveGame = false;
			engine->ServerCommand("sv_lockteams 0\n");  // Unlock teams
		}

		if (sv_overtime.GetBool() && sv_overtime_limit.GetInt() > iOvertimeLimit)
		{
			if (!IsTeamplay())
			{
				// Check for a tie before going to intermission
				int highestFrags = -1;
				int tieCount = 0;

				iOvertimeLimit++;

				// Loop through all players to find the highest frag count
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

					if (pPlayer && pPlayer->IsConnected())
					{
						int frags = pPlayer->FragCount();

						// If we find a higher frag count, reset the tie count and update the highest frags
						if (frags > highestFrags)
						{
							highestFrags = frags;
							tieCount = 1; // Reset to 1 because we found a new highest frag player
						}
						// If another player has the same frag count, increase the tie count
						else if (frags == highestFrags)
						{
							tieCount++;
						}
					}
				}

				// If we have a tie (2 or more players with the highest frag count)
				if (tieCount > 1)
				{
					// Add 1 minute to the game time (60 seconds)
					float flExtraTime = sv_overtime_time.GetInt() * 60.0f; // 1 minute in seconds

					// Use the same formula from GetMapRemainingTime() and add 60 seconds
					float currentRemainingTime = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f) - gpGlobals->curtime;

					// Add the extra time to the remaining time
					currentRemainingTime += flExtraTime;

					// Update m_flGameStartTime to reflect the new end time based on the remaining time
					m_flGameStartTime = gpGlobals->curtime - (mp_timelimit.GetInt() * 60.0f - currentRemainingTime);

					// Play overtime sound to all players
					for (int i = 1; i <= gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
						if (pPlayer && pPlayer->IsConnected())
						{
							CRecipientFilter filter;
							filter.AddRecipient(pPlayer);
							filter.MakeReliable();

							if (sv_custom_sounds.GetBool())
								CBaseEntity::EmitSound(filter, pPlayer->entindex(), "server_sounds_overtime");

							UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("+0%d:00 OVERTIME", sv_overtime_time.GetInt()));
						}
					}

					// Don't go to intermission just yet
					return;
				}
			}
			else
			{
				CTeam* pCombine = g_Teams[TEAM_COMBINE];
				CTeam* pRebels = g_Teams[TEAM_REBELS];

				if (pCombine->GetScore() == pRebels->GetScore())
				{
					// Add 1 minute to the game time (60 seconds)
					float flExtraTime = sv_overtime_time.GetInt() * 60.0f; // 1 minute in seconds

					// Use the same formula from GetMapRemainingTime() and add 60 seconds
					float currentRemainingTime = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f) - gpGlobals->curtime;

					// Add the extra time to the remaining time
					currentRemainingTime += flExtraTime;

					// Update m_flGameStartTime to reflect the new end time based on the remaining time
					m_flGameStartTime = gpGlobals->curtime - (mp_timelimit.GetInt() * 60.0f - currentRemainingTime);

					// Play overtime sound to all players
					for (int i = 1; i <= gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
						if (pPlayer && pPlayer->IsConnected())
						{
							CRecipientFilter filter;
							filter.AddRecipient(pPlayer);
							filter.MakeReliable();

							if (sv_custom_sounds.GetBool())
								CBaseEntity::EmitSound(filter, pPlayer->entindex(), "server_sounds_overtime");

							UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("+0%d:00 OVERTIME", sv_overtime_time.GetInt()));
						}
					}

					// Don't go to intermission just yet
					return;
				}
			}
		}

		// Else, go to intermission
		GoToIntermission();
		return;
	}

	if (flFragLimit)
	{
		if (IsTeamplay() == true)
		{
			CTeam* pCombine = g_Teams[TEAM_COMBINE];
			CTeam* pRebels = g_Teams[TEAM_REBELS];

			if (pCombine->GetScore() >= flFragLimit || pRebels->GetScore() >= flFragLimit)
			{
				GoToIntermission();
				return;
			}
		}
		else
		{
			// check if any player is over the frag limit
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

				if (pPlayer && pPlayer->FragCount() >= flFragLimit)
				{
					GoToIntermission();
					return;
				}
			}
		}
	}

	if (gpGlobals->curtime > m_flCopsVsFugitive)
	{
		m_flCopsVsFugitive = gpGlobals->curtime + 1.0;
	}

	if (gpGlobals->curtime > m_tmNextPeriodicThink)
	{
		CheckRestartGame();
		m_tmNextPeriodicThink = gpGlobals->curtime + 1.0;
	}

	if (gpGlobals->curtime > m_flBalanceTeamsTime)
		m_flBalanceTeamsTime = gpGlobals->curtime + 60.0;

	if (m_flRestartGameTime > 0.0f && m_flRestartGameTime <= gpGlobals->curtime)
	{
		RestartGame();
	}

	ManageObjectRelocation();

#endif
}

void CHL2MPRules::GoToIntermission(void)
{
#ifndef CLIENT_DLL
	if (g_fGameOver)
		return;

	g_fGameOver = true;

	m_flIntermissionEndTime = gpGlobals->curtime + mp_chattime.GetInt();

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		pPlayer->ShowViewPortPanel(PANEL_SCOREBOARD);
		pPlayer->AddFlag(FL_FROZEN);
	}
#endif

}

bool CHL2MPRules::CheckGameOver()
{
#ifndef CLIENT_DLL
	if (g_fGameOver)   // someone else quit the game already
	{
		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->curtime)
		{
			ChangeLevel(); // intermission is over	
		}

		return true;
	}
#endif

	return false;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHL2MPRules::FlWeaponTryRespawn(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	if (pWeapon && (pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD))
	{
		if (gEntList.NumberOfEntities() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE))
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime(pWeapon);
	}
#endif
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHL2MPRules::VecWeaponRespawnSpot(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	CWeaponHL2MPBase* pHL2Weapon = dynamic_cast<CWeaponHL2MPBase*>(pWeapon);

	if (pHL2Weapon)
	{
		return pHL2Weapon->GetOriginalSpawnOrigin();
	}
#endif

	return pWeapon->GetAbsOrigin();
}

QAngle CHL2MPRules::DefaultWeaponRespawnAngle(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	CWeaponHL2MPBase* pHL2Weapon = dynamic_cast<CWeaponHL2MPBase*>(pWeapon);

	if (pHL2Weapon)
	{
		return pHL2Weapon->GetOriginalSpawnAngles();
	}
#endif
	return pWeapon->GetAbsAngles();
}

#ifndef CLIENT_DLL

CItem* IsManagedObjectAnItem(CBaseEntity* pObject)
{
	return dynamic_cast<CItem*>(pObject);
}

CWeaponHL2MPBase* IsManagedObjectAWeapon(CBaseEntity* pObject)
{
	return dynamic_cast<CWeaponHL2MPBase*>(pObject);
}

bool GetObjectsOriginalParameters(CBaseEntity* pObject, Vector& vOriginalOrigin, QAngle& vOriginalAngles)
{
	if (CItem* pItem = IsManagedObjectAnItem(pObject))
	{
		if (pItem->m_flNextResetCheckTime > gpGlobals->curtime)
			return false;

		vOriginalOrigin = pItem->GetOriginalSpawnOrigin();
		vOriginalAngles = pItem->GetOriginalSpawnAngles();

		pItem->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_item_respawn_time.GetFloat();
		return true;
	}
	else if (CWeaponHL2MPBase* pWeapon = IsManagedObjectAWeapon(pObject))
	{
		if (pWeapon->m_flNextResetCheckTime > gpGlobals->curtime)
			return false;

		vOriginalOrigin = pWeapon->GetOriginalSpawnOrigin();
		vOriginalAngles = pWeapon->GetOriginalSpawnAngles();

		pWeapon->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_weapon_respawn_time.GetFloat();
		return true;
	}

	return false;
}

void CHL2MPRules::ManageObjectRelocation(void)
{
	int iTotal = m_hRespawnableItemsAndWeapons.Count();

	if (iTotal > 0)
	{
		for (int i = 0; i < iTotal; i++)
		{
			CBaseEntity* pObject = m_hRespawnableItemsAndWeapons[i].Get();

			if (pObject)
			{
				Vector vSpawOrigin;
				QAngle vSpawnAngles;

				if (GetObjectsOriginalParameters(pObject, vSpawOrigin, vSpawnAngles) == true)
				{
					float flDistanceFromSpawn = (pObject->GetAbsOrigin() - vSpawOrigin).Length();

					if (flDistanceFromSpawn > WEAPON_MAX_DISTANCE_FROM_SPAWN)
					{
						bool shouldReset = false;
						IPhysicsObject* pPhysics = pObject->VPhysicsGetObject();

						if (pPhysics)
						{
							shouldReset = pPhysics->IsAsleep();
						}
						else
						{
							shouldReset = (pObject->GetFlags() & FL_ONGROUND) ? true : false;
						}

						if (shouldReset)
						{
							pObject->Teleport(&vSpawOrigin, &vSpawnAngles, NULL);
							pObject->EmitSound("AlyxEmp.Charge");

							IPhysicsObject* pPhys = pObject->VPhysicsGetObject();

							if (pPhys)
							{
								pPhys->Wake();
							}
						}
					}
				}
			}
		}
	}
}

//=========================================================
//AddLevelDesignerPlacedWeapon
//=========================================================
void CHL2MPRules::AddLevelDesignerPlacedObject(CBaseEntity* pEntity)
{
	if (m_hRespawnableItemsAndWeapons.Find(pEntity) == -1)
	{
		m_hRespawnableItemsAndWeapons.AddToTail(pEntity);
	}
}

//=========================================================
//RemoveLevelDesignerPlacedWeapon
//=========================================================
void CHL2MPRules::RemoveLevelDesignerPlacedObject(CBaseEntity* pEntity)
{
	if (m_hRespawnableItemsAndWeapons.Find(pEntity) != -1)
	{
		m_hRespawnableItemsAndWeapons.FindAndRemove(pEntity);
	}
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHL2MPRules::VecItemRespawnSpot(CItem* pItem)
{
	return pItem->GetOriginalSpawnOrigin();
}

//=========================================================
// What angles should this item use to respawn?
//=========================================================
QAngle CHL2MPRules::VecItemRespawnAngles(CItem* pItem)
{
	return pItem->GetOriginalSpawnAngles();
}

//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHL2MPRules::FlItemRespawnTime(CItem* pItem)
{
	return sv_hl2mp_item_respawn_time.GetFloat();
}


//=========================================================
// CanHaveWeapon - returns false if the player is not allowed
// to pick up this weapon
//=========================================================
bool CHL2MPRules::CanHavePlayerItem(CBasePlayer* pPlayer, CBaseCombatWeapon* pItem)
{
	if (weaponstay.GetInt() > 0)
	{
		if (pPlayer->Weapon_OwnsThisType(pItem->GetClassname(), pItem->GetSubType()))
			return false;
	}

	return BaseClass::CanHavePlayerItem(pPlayer, pItem);
}

#endif

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHL2MPRules::WeaponShouldRespawn(CBaseCombatWeapon* pWeapon)
{
#ifndef CLIENT_DLL
	if (pWeapon->HasSpawnFlags(SF_NORESPAWN))
	{
		return GR_WEAPON_RESPAWN_NO;
	}
#endif

	return GR_WEAPON_RESPAWN_YES;
}

//-----------------------------------------------------------------------------
// Purpose: Player has just left the game
//-----------------------------------------------------------------------------
void CHL2MPRules::ClientDisconnected(edict_t* pClient)
{
#ifndef CLIENT_DLL
	// Msg( "CLIENT DISCONNECTED, REMOVING FROM TEAM.\n" );

	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pClient);
	if (pPlayer)
	{
		// Remove the player from his team
		if (pPlayer->GetTeam())
		{
			pPlayer->GetTeam()->RemovePlayer(pPlayer);
		}
	}

	BaseClass::ClientDisconnected(pClient);

#endif
}

//=========================================================
// Deathnotice. 
//=========================================================
#ifndef CLIENT_DLL
void CHL2MP_Player::DelayedLeaderCheck()
{
	if (sv_custom_sounds.GetBool())
	{
		if (!HL2MPRules()->IsTeamplay())
		{
			CBasePlayer* pLeader = nullptr;
			int maxFrags = -1;
			int secondMaxFrags = -1; // Track the second-highest frag count
			int leadCount = 0; // Track if more than one player is tied for the lead

			// Iterate over all players to determine the leader
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				if (pPlayer)
				{
					int frags = pPlayer->FragCount();

					if (frags > maxFrags)
					{
						secondMaxFrags = maxFrags; // Keep track of the second-highest frag count
						maxFrags = frags;
						pLeader = pPlayer;
						leadCount = 1; // Reset lead count since we found a new leader
					}
					else if (frags == maxFrags)
					{
						leadCount++; // Increment the count of players with the same frag count
					}
					else if (frags > secondMaxFrags)
					{
						secondMaxFrags = frags;
					}
				}
			}

			// If there's more than one player tied for the lead, there's no leader
			if (leadCount > 1)
			{
				pLeader = nullptr;
			}

			// Iterate over all players and update their leader status
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

				if (pPlayer)
				{
					// If the player is the leader and has at least 1 frag lead, set their leader status
					if (pPlayer == pLeader && (maxFrags - secondMaxFrags) > 0)
					{
						if (!pPlayer->IsLeader()) // Only play the sound if their status changes
						{
							pPlayer->SetLeaderStatus(true);
							CRecipientFilter filter;
							filter.AddRecipient(pPlayer);
							filter.MakeReliable();
							CBaseEntity::EmitSound(filter, pPlayer->entindex(), "server_sounds_youlead");
						}
					}
					else
					{
						// If the player is not the leader or lost the lead, reset their leader status and play the sound
						if (pPlayer->IsLeader())
						{
							pPlayer->SetLeaderStatus(false);
							CRecipientFilter filter;
							filter.AddRecipient(pPlayer);
							filter.MakeReliable();
							CBaseEntity::EmitSound(filter, pPlayer->entindex(), "server_sounds_younolead");
						}
					}
				}
			}

			// Clear the think context after execution
			SetThink(nullptr);
		}
		else
		{
			// We could definitely fetch the team scores directly with GetTeam(), 
			// but we are doing it this way as a safety net 
			// just to be sure we have accurate frag counts

			// Initialize team variables
			int combineScore = 0;
			int rebelsScore = 0;

			// Iterate over all players to accumulate scores for both teams
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

				if (pPlayer && !pPlayer->IsObserver()) // Exclude spectators
				{
					if (pPlayer->GetTeamNumber() == TEAM_COMBINE)
					{
						combineScore += pPlayer->FragCount(); // Accumulate Combine team score
					}
					else if (pPlayer->GetTeamNumber() == TEAM_REBELS)
					{
						rebelsScore += pPlayer->FragCount(); // Accumulate Rebels team score
					}
				}
			}

			int currentLeaderTeam = TEAM_UNASSIGNED; // Variable to track current leader

			// Determine which team is leading
			if (rebelsScore > combineScore)
			{
				currentLeaderTeam = TEAM_REBELS;
			}
			else if (combineScore > rebelsScore)
			{
				currentLeaderTeam = TEAM_COMBINE;
			}

			// Only play the sound if there's a change in the leader
			if (currentLeaderTeam != g_iPreviousLeaderTeam)
			{
				if (currentLeaderTeam == TEAM_REBELS)
				{
					// Rebels take the lead, play the sound
					for (int i = 0; i < gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
						if (pPlayer && !g_bCopsVsFugitiveGame)
						{
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/red_leads.wav\n");
						}
					}
				}
				else if (currentLeaderTeam == TEAM_COMBINE)
				{
					// Combine take the lead, play the sound
					for (int i = 0; i < gpGlobals->maxClients; i++)
					{
						CBasePlayer* pPlayer = UTIL_PlayerByIndex(i + 1);
						if (pPlayer && !g_bCopsVsFugitiveGame)
						{
							engine->ClientCommand(pPlayer->edict(), "play server_sounds/blue_leads.wav\n");
						}
					}
				}

				// Update the previous leader team
				g_iPreviousLeaderTeam = currentLeaderTeam;
			}

			// If the scores are tied, do nothing (no leader change)
			if (rebelsScore == combineScore)
			{
				g_iPreviousLeaderTeam = TEAM_UNASSIGNED; // No leader during a tie
			}

			SetThink(nullptr);
		}
	}
}
#endif

void CHL2MPRules::DeathNotice(CBasePlayer* pVictim, const CTakeDamageInfo& info)
{
#ifndef CLIENT_DLL
	// Work out what killed the player, and send a message to all clients about it
	const char* killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_ID = 0;

	// Find the killer & the scorer
	CBaseEntity* pInflictor = info.GetInflictor();
	CBaseEntity* pKiller = info.GetAttacker();
	CBasePlayer* pScorer = GetDeathScorer(pKiller, pInflictor);
	CBasePlayer* pVictimPlayer = dynamic_cast<CBasePlayer*>(pVictim);

	CHL2MP_Player* pAttackerPlayer = dynamic_cast<CHL2MP_Player*>(pScorer);
	CHL2MP_Player* pVictimHL2MP = dynamic_cast<CHL2MP_Player*>(pVictimPlayer);

	int victimUserID = pVictim->GetUserID();

	// Custom kill type?
	if (info.GetDamageCustom())
	{
		killer_weapon_name = GetDamageCustomString(info);
		if (pScorer)
		{
			killer_ID = pScorer->GetUserID();
		}
	}
	else
	{
		// Is the killer a client?
		if (pScorer)
		{
			killer_ID = pScorer->GetUserID();

			if (pInflictor)
			{
				if (pInflictor == pScorer)
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if (pScorer->GetActiveWeapon())
					{
						killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname();
					}
				}
				else
				{
					killer_weapon_name = pInflictor->GetClassname();  // it's just that easy
				}
			}
		}
		else
		{
			killer_weapon_name = pInflictor->GetClassname();
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if (strncmp(killer_weapon_name, "weapon_", 7) == 0)
		{
			killer_weapon_name += 7;
		}
		else if (strncmp(killer_weapon_name, "npc_", 4) == 0)
		{
			killer_weapon_name += 4;
		}
		else if (strncmp(killer_weapon_name, "func_", 5) == 0)
		{
			killer_weapon_name += 5;
		}
		else if (strstr(killer_weapon_name, "physics"))
		{
			killer_weapon_name = "physics";
		}
		if (strstr(killer_weapon_name, "physbox"))
		{
			killer_weapon_name = "physics";
		}
		if (strcmp(killer_weapon_name, "prop_combine_ball") == 0)
		{
			killer_weapon_name = "combine_ball";
		}
		else if (strcmp(killer_weapon_name, "grenade_ar2") == 0)
		{
			killer_weapon_name = "smg1_grenade";
		}
		else if (strcmp(killer_weapon_name, "satchel") == 0 || strcmp(killer_weapon_name, "tripmine") == 0)
		{
			killer_weapon_name = "slam";
		}
	}

	// Check if the attacker is valid and not the same as the victim
	if (pAttackerPlayer && pAttackerPlayer != pVictimHL2MP)
	{
		if (HL2MPRules()->IsTeamplay() && pAttackerPlayer->GetTeamNumber() == pVictimHL2MP->GetTeamNumber())
		{
			if (sv_teamkill_kick.GetBool())
			{
				pAttackerPlayer->IncrementTeamKillCount();

				if (sv_teamkill_kick_warning.GetBool())
					UTIL_PrintToClient(pAttackerPlayer, UTIL_VarArgs(CHAT_RED "You will be kicked if you kill %d more teammate%s!", sv_teamkill_kick_threshold.GetInt() - pAttackerPlayer->GetTeamKillCount(), (sv_teamkill_kick_threshold.GetInt() - pAttackerPlayer->GetTeamKillCount()) < 2 ? "" : "s"));

				if (pAttackerPlayer->GetTeamKillCount() >= sv_teamkill_kick_threshold.GetInt())
				{
					engine->ServerCommand(UTIL_VarArgs("kickid %d You were kicked for excessive team killing\n", pAttackerPlayer->GetUserID()));
					return;
				}
			}
		}

		if (HL2MPRules()->IsTeamplay() && pAttackerPlayer->GetTeamNumber() == pVictimHL2MP->GetTeamNumber())
		{
			CTeam* pKillerTeam = pAttackerPlayer->GetTeam();
			if (pKillerTeam)
			{
				pKillerTeam->AddScore(-2);
			}
		}

		// Increment the attacker's kill streak for this specific victim
		pAttackerPlayer->IncrementConsecutiveKillsForVictim(victimUserID);

		// Print domination message if the attacker has killed the victim 4 times in a row
		int killsOnVictim = pAttackerPlayer->GetConsecutiveKillsForVictim(victimUserID);
		if (killsOnVictim == 4 && sv_domination_messages.GetBool())
		{
			UTIL_PrintToAllClients(UTIL_VarArgs(CHAT_DEFAULT "%s " CHAT_CONTEXT "is dominating " CHAT_DEFAULT "%s" CHAT_CONTEXT "!\n", pAttackerPlayer->GetPlayerName(), pVictimHL2MP->GetPlayerName()));
		}

		// Reset the victim's kill streak (for this attacker)
		if (pVictimHL2MP)
		{
			pVictimHL2MP->ResetConsecutiveKillsForVictim(pAttackerPlayer->GetUserID());
		}

		// Play sounds based on hitgroup (headshot or body)
		if (pAttackerPlayer->AreKillSoundsEnabled() && sv_custom_sounds.GetBool())
		{
			CRecipientFilter filter;
			filter.AddRecipient(pAttackerPlayer);
			filter.MakeReliable();

			if (pAttackerPlayer->GetTeamNumber() == pVictimHL2MP->GetTeamNumber() && IsTeamplay())
			{
				CBaseEntity::EmitSound(filter, pAttackerPlayer->entindex(), "server_sounds_tkill");
			}
			else
			{
				// Use trace logic to determine the final hitgroup
				trace_t trace;
				Vector vecStart = info.GetDamagePosition();
				Vector vecEnd = pVictim->GetAbsOrigin();

				// Trace line for precise hit detection
				UTIL_TraceLine(vecStart, vecEnd, MASK_SHOT, pAttackerPlayer, COLLISION_GROUP_NONE, &trace);
				int hitgroup = trace.hitgroup;

				// Fall back to trace hull if necessary
				if (hitgroup == 0)
				{
					Vector mins(-1.5f, -1.5f, -1.5f);
					Vector maxs(1.5f, 1.5f, 1.5f);
					UTIL_TraceHull(vecStart, vecEnd, mins, maxs, MASK_SHOT, pAttackerPlayer, COLLISION_GROUP_NONE, &trace);
					hitgroup = trace.hitgroup;
				}

				if (hitgroup == HITGROUP_HEAD)
				{
					// Play headshot sound
					CBaseEntity::EmitSound(filter, pAttackerPlayer->entindex(), "headshot_kill_snd");
				}
				else
				{
					// Play regular frag sound
					CBaseEntity::EmitSound(filter, pAttackerPlayer->entindex(), "frag_snd");
				}
			}
		}
	}

	if (pAttackerPlayer)
	{
		// Delay the leader check by 1 second to ensure frags are updated
		pAttackerPlayer->SetContextThink(&CHL2MP_Player::DelayedLeaderCheck, gpGlobals->curtime + 0.2f, "DelayedLeaderCheck");
	}

	if (pVictimHL2MP && pVictimHL2MP == pAttackerPlayer)
	{
		// If the attacker is the one who died, reset their streaks
		pVictimHL2MP->ResetAllConsecutiveKills();
	}

	if (pAttackerPlayer && pVictimHL2MP && pAttackerPlayer == pVictimHL2MP && g_bCopsVsFugitiveGame)
	{
		// Check if the victim is the fugitive (on the rebels team)
		if (pVictimHL2MP->GetTeamNumber() == TEAM_REBELS)
		{
			// Switch the victim back to the cops
			pVictimHL2MP->ChangeTeam(TEAM_COMBINE);
			pVictimHL2MP->SetFugitiveStatus(false);
			pVictimHL2MP->SetTimerActive(false);

			// Find a new random fugitive (player on the cops team), excluding the victim
			CHL2MP_Player* pNewFugitive = nullptr;
			int iCombineCount = 0;

			// Loop through all players to count cops and pick a random one to be the new fugitive
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CHL2MP_Player* pPlayer = dynamic_cast<CHL2MP_Player*>(UTIL_PlayerByIndex(i));

				// Ensure the player is alive, on the cops team, and NOT the victim
				if (pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_COMBINE && pPlayer != pVictimHL2MP)
				{
					iCombineCount++;

					// Pick a random player on the cops team as the new fugitive
					if (!pNewFugitive || random->RandomInt(0, iCombineCount - 1) == 0)
					{
						pNewFugitive = pPlayer;
					}
				}
			}

			// If we found a new fugitive, switch their team to fugitive
			if (pNewFugitive)
			{
				pNewFugitive->ChangeTeam(TEAM_REBELS);
				pNewFugitive->SetFugitiveStatus(true);
				pNewFugitive->SetTimerActive(true);
				UTIL_PrintToAllClients(CHAT_DEFAULT "%s1 " CHAT_CONTEXT "is the fugitive!", pNewFugitive->GetPlayerName());
			}
		}
	}

	if (pAttackerPlayer && pVictimHL2MP && pAttackerPlayer != pVictimHL2MP && g_bCopsVsFugitiveGame)
	{
		// Check if the victim is the fugitive
		if (pVictimHL2MP->GetTeamNumber() == TEAM_REBELS)
		{
			pVictimHL2MP->SetFugitiveStatus(false);
			pVictimHL2MP->SetTimerActive(false);

			pAttackerPlayer->SetFugitiveStatus(true);
			pAttackerPlayer->SetTimerActive(true);

			// The attacker becomes the new fugitive
			pAttackerPlayer->ChangeTeam(TEAM_REBELS);

			UTIL_PrintToAllClients(CHAT_DEFAULT "%s1 " CHAT_CONTEXT "is the fugitive!", pAttackerPlayer->GetPlayerName());

			// The victim becomes a cop
			pVictimHL2MP->ChangeTeam(TEAM_COMBINE);
		}
	}

	IGameEvent* event = gameeventmanager->CreateEvent("player_death");
	if (event)
	{
		event->SetInt("userid", pVictim->GetUserID());
		event->SetInt("attacker", killer_ID);
		event->SetString("weapon", killer_weapon_name);
		event->SetInt("priority", 7);
		gameeventmanager->FireEvent(event);
	}
#endif

}

void CHL2MPRules::ClientSettingsChanged(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL

	CHL2MP_Player* pHL2Player = ToHL2MPPlayer(pPlayer);

	if (pHL2Player == NULL)
		return;

	const char* pCurrentModel = modelinfo->GetModelName(pPlayer->GetModel());
	const char* szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(pPlayer->edict()), "cl_playermodel");

	//If we're different.
	if (stricmp(szModelName, pCurrentModel))
	{
		//Too soon, set the cvar back to what it was.
		//Note: this will make this function be called again
		//but since our models will match it'll just skip this whole dealio.
		if (pHL2Player->GetNextModelChangeTime() >= gpGlobals->curtime)
		{
			char szReturnString[512];

			Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", pCurrentModel);
			engine->ClientCommand(pHL2Player->edict(), szReturnString);
			return;
		}

		if (HL2MPRules()->IsTeamplay() == false)
		{
			pHL2Player->SetPlayerTeamModel();
		}
		else
		{
			if (Q_stristr(szModelName, "models/human"))
			{
				pHL2Player->ChangeTeam(TEAM_REBELS);
			}
			else
			{
				pHL2Player->SetPlayerTeamModel();
			}
		}
	}
	if (sv_report_client_settings.GetInt() == 1)
	{
		UTIL_LogPrintf("\"%s\" cl_cmdrate = \"%s\"\n", pHL2Player->GetPlayerName(), engine->GetClientConVarValue(pHL2Player->entindex(), "cl_cmdrate"));
	}

	BaseClass::ClientSettingsChanged(pPlayer);
#endif

}

int CHL2MPRules::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
#ifndef CLIENT_DLL
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if (!pPlayer || !pTarget || !pTarget->IsPlayer() || IsTeamplay() == false)
		return GR_NOTTEAMMATE;

	if ((*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp(GetTeamID(pPlayer), GetTeamID(pTarget)))
	{
		return GR_TEAMMATE;
	}
#endif

	return GR_NOTTEAMMATE;
}

const char* CHL2MPRules::GetGameDescription(void)
{
	if (IsTeamplay())
		return "Team Deathmatch";

	return "Deathmatch";
}

bool CHL2MPRules::IsConnectedUserInfoChangeAllowed(CBasePlayer* pPlayer)
{
	return true;
}

float CHL2MPRules::GetMapRemainingTime()
{
	// if timelimit is disabled, return 0
	if (mp_timelimit.GetInt() <= 0)
		return 0;

	// timelimit is in minutes

	float timeleft = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f) - gpGlobals->curtime;

	return timeleft;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPRules::Precache(void)
{
	CBaseEntity::PrecacheScriptSound("AlyxEmp.Charge");
}

bool CHL2MPRules::ShouldCollide(int collisionGroup0, int collisionGroup1)
{
	if (collisionGroup0 > collisionGroup1)
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0, collisionGroup1);
	}

	if ((collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_WEAPON)
	{
		return false;
	}

	return BaseClass::ShouldCollide(collisionGroup0, collisionGroup1);

}

bool CHL2MPRules::ClientCommand(CBaseEntity* pEdict, const CCommand& args)
{
#ifndef CLIENT_DLL
	if (BaseClass::ClientCommand(pEdict, args))
		return true;


	CHL2MP_Player* pPlayer = (CHL2MP_Player*)pEdict;

	if (pPlayer->ClientCommand(args))
		return true;
#endif

	return false;
}

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			3.5
// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if (!bInitted)
	{
		bInitted = true;

		def.AddAmmoType("AR2", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 60, BULLET_IMPULSE(200, 1225), 0);
		def.AddAmmoType("AR2AltFire", DMG_DISSOLVE, TRACER_NONE, 0, 0, 3, 0, 0);
		def.AddAmmoType("Pistol", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 150, BULLET_IMPULSE(200, 1225), 0);
		def.AddAmmoType("SMG1", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 225, BULLET_IMPULSE(200, 1225), 0);
		def.AddAmmoType("357", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, 12, BULLET_IMPULSE(800, 5000), 0);
		def.AddAmmoType("XBowBolt", DMG_BULLET, TRACER_LINE, 0, 0, 10, BULLET_IMPULSE(800, 8000), 0);
		def.AddAmmoType("Buckshot", DMG_BULLET | DMG_BUCKSHOT, TRACER_LINE, 0, 0, 30, BULLET_IMPULSE(400, 1200), 0);
		def.AddAmmoType("RPG_Round", DMG_BURN, TRACER_NONE, 0, 0, 3, 0, 0);
		def.AddAmmoType("SMG1_Grenade", DMG_BURN, TRACER_NONE, 0, 0, 3, 0, 0);
		def.AddAmmoType("Grenade", DMG_BURN, TRACER_NONE, 0, 0, 5, 0, 0);
		def.AddAmmoType("slam", DMG_BURN, TRACER_NONE, 0, 0, 5, 0, 0);
	}

	return &def;
}

#ifdef CLIENT_DLL

ConVar cl_autowepswitch(
	"cl_autowepswitch",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Automatically switch to picked up weapons (if more powerful)");

#else

#ifdef DEBUG

// Handler for the "bot" command.
void Bot_f()
{
	// Look at -count.
	int count = 1;
	count = clamp(count, 1, 16);

	int iTeam = TEAM_COMBINE;

	// Look at -frozen.
	bool bFrozen = false;

	// Ok, spawn all the bots.
	while (--count >= 0)
	{
		BotPutInServer(bFrozen, iTeam);
	}
}


ConCommand cc_Bot("bot", Bot_f, "Add a bot.", FCVAR_CHEAT);

#endif

bool CHL2MPRules::FShouldSwitchWeapon(CBasePlayer* pPlayer, CBaseCombatWeapon* pWeapon)
{
	if (pPlayer->GetActiveWeapon() && pPlayer->IsNetClient())
	{
		// Player has an active item, so let's check cl_autowepswitch.
		const char* cl_autowepswitch = engine->GetClientConVarValue(engine->IndexOfEdict(pPlayer->edict()), "cl_autowepswitch");
		if (cl_autowepswitch && atoi(cl_autowepswitch) <= 0)
		{
			return false;
		}
	}

	return BaseClass::FShouldSwitchWeapon(pPlayer, pWeapon);
}

#endif

#ifndef CLIENT_DLL

void CHL2MPRules::NewRestartGame()
{
	// bounds check
	if (mp_timelimit.GetInt() < 0)
	{
		mp_timelimit.SetValue(0);
	}
	m_flGameStartTime = gpGlobals->curtime;
	if (!IsFinite(m_flGameStartTime.Get()))
	{
		Warning("Trying to set a NaN game start time\n");
		m_flGameStartTime.GetForModify() = 0.0f;
	}

	float flTimelimit = mp_timelimit.GetInt();

	float currentRemainingTime = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f) - gpGlobals->curtime;

	currentRemainingTime += flTimelimit;

	m_flGameStartTime = gpGlobals->curtime - (mp_timelimit.GetInt() * 60.0f - currentRemainingTime);

	g_fGameOver = false;

	CleanUpMap();

	// now respawn all players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		if (pPlayer->GetActiveWeapon())
		{
			pPlayer->GetActiveWeapon()->Holster();
		}
		pPlayer->RemoveAllItems(true);
		respawn(pPlayer, false);
		pPlayer->Reset();
		pPlayer->SetLeaderStatus(false);
		pPlayer->ResetTeamKillCount();
		pPlayer->ResetAllConsecutiveKills();
	}

	CTeam* pRebels = GetGlobalTeam(TEAM_REBELS);
	CTeam* pCombine = GetGlobalTeam(TEAM_COMBINE);

	if (pRebels)
	{
		pRebels->SetScore(0);
	}

	if (pCombine)
	{
		pCombine->SetScore(0);
	}

	if (g_bCopsVsFugitive)
		engine->ServerCommand("sv_lockteams 1\n");

	m_flIntermissionEndTime = 0;
	m_flRestartGameTime = 0.0;
	m_bCompleteReset = false;

	m_flGameStartTime = 0;

	m_hRespawnableItemsAndWeapons.RemoveAll();
	m_tmNextPeriodicThink = 0;
	m_flCopsVsFugitive = 0;
	m_bChangelevelDone = false;
	m_bFirstInitialization = false;
	g_bGameOverSounds = false;

	iOvertimeLimit = 0;

	m_flBalanceTeamsTime = 0.0f;

	return;
}

void CHL2MPRules::RestartGame()
{
	// bounds check
	if (mp_timelimit.GetInt() < 0)
	{
		mp_timelimit.SetValue(0);
	}
	m_flGameStartTime = gpGlobals->curtime;
	if (!IsFinite(m_flGameStartTime.Get()))
	{
		Warning("Trying to set a NaN game start time\n");
		m_flGameStartTime.GetForModify() = 0.0f;
	}

	iOvertimeLimit = 0;

	CleanUpMap();

	// now respawn all players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		if (pPlayer->GetActiveWeapon())
		{
			pPlayer->GetActiveWeapon()->Holster();
		}
		pPlayer->RemoveAllItems(true);
		respawn(pPlayer, false);
		pPlayer->Reset();
		pPlayer->SetLeaderStatus(false);
		pPlayer->ResetTeamKillCount();
		pPlayer->ResetAllConsecutiveKills();
	}

	// Respawn entities (glass, doors, etc..)

	CTeam* pRebels = GetGlobalTeam(TEAM_REBELS);
	CTeam* pCombine = GetGlobalTeam(TEAM_COMBINE);

	if (pRebels)
	{
		pRebels->SetScore(0);
	}

	if (pCombine)
	{
		pCombine->SetScore(0);
	}

	m_flIntermissionEndTime = 0;
	m_flRestartGameTime = 0.0;
	m_bCompleteReset = false;

	IGameEvent* event = gameeventmanager->CreateEvent("round_start");
	if (event)
	{
		event->SetInt("fraglimit", 0);
		event->SetInt("priority", 6); // HLTV event priority, not transmitted

		event->SetString("objective", "DEATHMATCH");

		gameeventmanager->FireEvent(event);
	}
}

void CHL2MPRules::CleanUpMap()
{
	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.

	// Get rid of all entities except players.
	CBaseEntity* pCur = gEntList.FirstEnt();
	while (pCur)
	{
		CBaseHL2MPCombatWeapon* pWeapon = dynamic_cast<CBaseHL2MPCombatWeapon*>(pCur);
		// Weapons with owners don't want to be removed..
		if (pWeapon)
		{
			if (!pWeapon->GetPlayerOwner())
			{
				UTIL_Remove(pCur);
			}
		}
		// remove entities that has to be restored on roundrestart (breakables etc)
		else if (!FindInList(s_PreserveEnts, pCur->GetClassname()))
		{
			UTIL_Remove(pCur);
		}

		pCur = gEntList.NextEnt(pCur);
	}

	// Really remove the entities so we can have access to their slots below.
	gEntList.CleanupDeleteList();

	// Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
	// could kill respawning CTs
	g_EventQueue.Clear();

	// Now reload the map entities.
	class CHL2MPMapEntityFilter : public IMapEntityFilter
	{
	public:
		virtual bool ShouldCreateEntity(const char* pClassname)
		{
			// Don't recreate the preserved entities.
			if (!FindInList(s_PreserveEnts, pClassname))
			{
				return true;
			}
			else
			{
				// Increment our iterator since it's not going to call CreateNextEntity for this ent.
				if (m_iIterator != g_MapEntityRefs.InvalidIndex())
					m_iIterator = g_MapEntityRefs.Next(m_iIterator);

				return false;
			}
		}


		virtual CBaseEntity* CreateNextEntity(const char* pClassname)
		{
			if (m_iIterator == g_MapEntityRefs.InvalidIndex())
			{
				// This shouldn't be possible. When we loaded the map, it should have used 
				// CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
				// with the same list of entities we're referring to here.
				Assert(false);
				return NULL;
			}
			else
			{
				CMapEntityRef& ref = g_MapEntityRefs[m_iIterator];
				m_iIterator = g_MapEntityRefs.Next(m_iIterator);	// Seek to the next entity.

				if (ref.m_iEdict == -1 || engine->PEntityOfEntIndex(ref.m_iEdict))
				{
					// Doh! The entity was delete and its slot was reused.
					// Just use any old edict slot. This case sucks because we lose the baseline.
					return CreateEntityByName(pClassname);
				}
				else
				{
					// Cool, the slot where this entity was is free again (most likely, the entity was 
					// freed above). Now create an entity with this specific index.
					return CreateEntityByName(pClassname, ref.m_iEdict);
				}
			}
		}

	public:
		int m_iIterator; // Iterator into g_MapEntityRefs.
	};
	CHL2MPMapEntityFilter filter;
	filter.m_iIterator = g_MapEntityRefs.Head();

	// DO NOT CALL SPAWN ON info_node ENTITIES!

	MapEntity_ParseAllEntities(engine->GetMapEntitiesString(), &filter, true);
}

void CHL2MPRules::CheckChatForReadySignal(CHL2MP_Player* pPlayer, const char* chatmsg)
{
	if (m_bAwaitingReadyRestart && FStrEq(chatmsg, mp_ready_signal.GetString()))
	{
		if (!pPlayer->IsReady())
		{
			pPlayer->SetReady(true);
		}
	}
}

extern ConVar mp_restartgame_immediate;

void CHL2MPRules::CheckRestartGame(void)
{
	if (g_fGameOver)
		return;

	if (mp_restartgame_immediate.GetBool())
	{
		m_flRestartGameTime = gpGlobals->curtime;
		m_bCompleteReset = true;
		mp_restartgame.SetValue(0);
		mp_restartgame_immediate.SetValue(0);
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Game has been restarted!");
	}
	else
	{
		// Restart the game if specified by the server
		int iRestartDelay = mp_restartgame.GetInt();

		if (iRestartDelay > 0)
		{
			if (iRestartDelay > 60)
				iRestartDelay = 60;

			// let the players know

			char strRestartDelay[64];
			Q_snprintf(strRestartDelay, sizeof(strRestartDelay), "%d", iRestartDelay);
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS");
			UTIL_ClientPrintAll(HUD_PRINTCONSOLE, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS");

			m_flRestartGameTime = gpGlobals->curtime + iRestartDelay;
			m_bCompleteReset = true;
			mp_restartgame.SetValue(0);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CHL2MPRules::GetChatFormat(bool bTeamOnly, CBasePlayer* pPlayer)
{
	if (!pPlayer)  // dedicated server output
	{
		return NULL;
	}

	const char* pszFormat = NULL;

	// team only
	if (bTeamOnly == TRUE)
	{
		if (pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
		{
			pszFormat = "HL2MP_Chat_Spec";
		}
		else
		{
			const char* chatLocation = GetChatLocation(bTeamOnly, pPlayer);
			if (chatLocation && *chatLocation)
			{
				pszFormat = "HL2MP_Chat_Team_Loc";
			}
			else
			{
				pszFormat = "HL2MP_Chat_Team";
			}
		}
	}
	// everyone
	else
	{
		if (pPlayer->GetTeamNumber() != TEAM_SPECTATOR)
		{
			pszFormat = "HL2MP_Chat_All";
		}
		else
		{
			pszFormat = "HL2MP_Chat_AllSpec";
		}
	}

	return pszFormat;
}

#endif