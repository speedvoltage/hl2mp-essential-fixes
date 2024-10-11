//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "hl2mp_player.h"
#include "globalstate.h"
#include "game.h"
#include "gamerules.h"
#include "hl2mp_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "hl2mp_gamerules.h"
#include "KeyValues.h"
#include "team.h"
#include "weapon_hl2mpbase.h"
#include "grenade_satchel.h"
#include "grenade_tripmine.h"
#include "eventqueue.h"
#include "gamestats.h"
#include "hl2mp_cvars.h"
#include "iserver.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#include "ilagcompensationmanager.h"

int g_iLastCitizenModel = 0;
int g_iLastCombineModel = 0;

CBaseEntity	 *g_pLastCombineSpawn = NULL;
CBaseEntity	 *g_pLastRebelSpawn = NULL;
extern CBaseEntity				*g_pLastSpawn;
extern IServerPluginHelpers* serverpluginhelpers;

#define HL2MP_COMMAND_MAX_RATE 0.3

#ifdef HL2MP
extern CSuitPowerDevice SuitDeviceFlashlight;
#endif

void DropPrimedFragGrenade( CHL2MP_Player *pPlayer, CBaseCombatWeapon *pGrenade );

LINK_ENTITY_TO_CLASS( player, CHL2MP_Player );

LINK_ENTITY_TO_CLASS( info_player_combine, CPointEntity );
LINK_ENTITY_TO_CLASS( info_player_rebel, CPointEntity );

IMPLEMENT_SERVERCLASS_ST(CHL2MP_Player, DT_HL2MP_Player)
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN ),
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),
	SendPropInt( SENDINFO( m_iSpawnInterpCounter), 4 ),
	SendPropInt( SENDINFO( m_iPlayerSoundType), 3 ),
	
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),

//	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
//	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),
	
END_SEND_TABLE()

BEGIN_DATADESC( CHL2MP_Player )
END_DATADESC()

const char *g_ppszRandomCitizenModels[] = 
{
	"models/humans/group03/male_01.mdl",
	"models/humans/group03/male_02.mdl",
	"models/humans/group03/female_01.mdl",
	"models/humans/group03/male_03.mdl",
	"models/humans/group03/female_02.mdl",
	"models/humans/group03/male_04.mdl",
	"models/humans/group03/female_03.mdl",
	"models/humans/group03/male_05.mdl",
	"models/humans/group03/female_04.mdl",
	"models/humans/group03/male_06.mdl",
	"models/humans/group03/female_06.mdl",
	"models/humans/group03/male_07.mdl",
	"models/humans/group03/female_07.mdl",
	"models/humans/group03/male_08.mdl",
	"models/humans/group03/male_09.mdl",
};

const char *g_ppszRandomCombineModels[] =
{
	"models/combine_soldier.mdl",
	"models/combine_soldier_prisonguard.mdl",
	"models/combine_super_soldier.mdl",
	"models/police.mdl",
};

// To enable us to create menus
class CEmptyPluginCallbacks : public IServerPluginCallbacks
{
public:
	virtual bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override { return true; }
	virtual void Unload() override {}
	virtual void Pause() override {}
	virtual void UnPause() override {}
	virtual const char* GetPluginDescription() override { return "Empty Plugin Callback"; }
	virtual void LevelInit(const char* mapname) override {}
	virtual void LevelShutdown() override {}
	virtual void ClientActive(edict_t* pEntity) override {}
	virtual void ClientDisconnect(edict_t* pEntity) override {}
	virtual void ClientPutInServer(edict_t* pEntity, const char* playername) override {}
	virtual void SetCommandClient(int index) override {}
	virtual void ClientSettingsChanged(edict_t* pEdict) override {}
	virtual PLUGIN_RESULT ClientCommand(edict_t* pEntity, const CCommand& args) override { return PLUGIN_CONTINUE; }
	virtual PLUGIN_RESULT NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) override { return PLUGIN_CONTINUE; }
	virtual void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) override {}
	virtual void OnEdictAllocated(edict_t* edict) override {}
	virtual void OnEdictFreed(const edict_t* edict) override {}

	// Implement the missing pure virtual functions
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override {}
	virtual void GameFrame(bool simulating) override {}
	virtual PLUGIN_RESULT ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxRejectLen) override { return PLUGIN_CONTINUE; }
};

// Global instance of the empty plugin callback
CEmptyPluginCallbacks g_EmptyPluginCallbacks;

#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 0.1f
#define TEAM_CHANGE_INTERVAL 5.0f

#define HL2MPPLAYER_PHYSDAMAGE_SCALE 4.0f

#pragma warning( disable : 4355 )

extern int g_iPreviousLeaderTeam;

CHL2MP_Player::CHL2MP_Player() : m_PlayerAnimState( this )
{
	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_iSpawnInterpCounter = 0;

    m_bEnterObserver = false;

	m_bIsLeader = false;

	g_iPreviousLeaderTeam = TEAM_UNASSIGNED;

	Set357ZoomLevel(20);  // Default zoom level for .357
	SetXbowZoomLevel(20); // Default zoom level for crossbow
	SetHitSoundsEnabled(true);
	SetKillSoundsEnabled(true);

	BaseClass::ChangeTeam( 0 );
	
//	UseClientSideAnimation();
}

CHL2MP_Player::~CHL2MP_Player( void )
{

}

void CHL2MP_Player::UpdateOnRemove( void )
{
	if (auto p = GetLadderMove())
	{
		UTIL_Remove((CBaseEntity*)(p->m_hReservedSpot.Get()));
		p->m_hReservedSpot = NULL;
	}
	if ( m_hRagdoll )
	{
		UTIL_RemoveImmediate( m_hRagdoll );
		m_hRagdoll = NULL;
	}

	BaseClass::UpdateOnRemove();
}

void CHL2MP_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel ( "sprites/glow01.vmt" );

	//Precache Citizen models
	int nHeads = ARRAYSIZE( g_ppszRandomCitizenModels );
	int i;	

	for ( i = 0; i < nHeads; ++i )
	   	 PrecacheModel( g_ppszRandomCitizenModels[i] );

	//Precache Combine Models
	nHeads = ARRAYSIZE( g_ppszRandomCombineModels );

	for ( i = 0; i < nHeads; ++i )
	   	 PrecacheModel( g_ppszRandomCombineModels[i] );

	PrecacheFootStepSounds();

	PrecacheScriptSound( "NPC_MetroPolice.Die" );
	PrecacheScriptSound( "NPC_CombineS.Die" );
	PrecacheScriptSound( "NPC_Citizen.die" );

	if (sv_custom_sounds.GetBool())
	{
		PrecacheScriptSound("server_sounds_red_team");
		PrecacheScriptSound("server_sounds_red_leads");
		PrecacheScriptSound("server_sounds_blue_team");
		PrecacheScriptSound("server_sounds_blue_leads");
		PrecacheScriptSound("server_sounds_five");
		PrecacheScriptSound("server_sounds_four");
		PrecacheScriptSound("server_sounds_three");
		PrecacheScriptSound("server_sounds_two");
		PrecacheScriptSound("server_sounds_one");
		PrecacheScriptSound("server_sounds_gameover");
		PrecacheScriptSound("server_sounds_gamepaused");
		PrecacheScriptSound("server_sounds_go1");
		PrecacheScriptSound("server_sounds_go2");
		PrecacheScriptSound("server_sounds_go3");
		PrecacheScriptSound("server_sounds_matchcancel");
		PrecacheScriptSound("server_sounds_rankdown");
		PrecacheScriptSound("server_sounds_rankup");
		PrecacheScriptSound("server_sounds_tie");
		PrecacheScriptSound("server_sounds_overtime");
		PrecacheScriptSound("server_sounds_youlead");
		PrecacheScriptSound("server_sounds_youlost");
		PrecacheScriptSound("server_sounds_younolead");
		PrecacheScriptSound("server_sounds_youwin");
		PrecacheScriptSound("kevlar1");
		PrecacheScriptSound("kevlar2");
		PrecacheScriptSound("kevlar3");
		PrecacheScriptSound("kevlar4");
		PrecacheScriptSound("kevlar5");
		PrecacheScriptSound("server_sounds_bhit_helmet-1");
		PrecacheScriptSound("server_sounds_hitbody");
		PrecacheScriptSound("server_sounds_hithead");
		PrecacheScriptSound("headshot_kill_snd");
		PrecacheScriptSound("frag_snd");
		PrecacheScriptSound("server_sounds_tkill");
	}
}

void CHL2MP_Player::GiveAllItems( void )
{
	if (GetTeamNumber() != TEAM_SPECTATOR)
	{
		EquipSuit();

		CBasePlayer::GiveAmmo(255, "Pistol");
		CBasePlayer::GiveAmmo(255, "AR2");
		CBasePlayer::GiveAmmo(5, "AR2AltFire");
		CBasePlayer::GiveAmmo(255, "SMG1");
		CBasePlayer::GiveAmmo(3, "smg1_grenade");
		CBasePlayer::GiveAmmo(255, "Buckshot");
		CBasePlayer::GiveAmmo(32, "357");
		CBasePlayer::GiveAmmo(3, "rpg_round");
		CBasePlayer::GiveAmmo(10, "XBowBolt");

		CBasePlayer::GiveAmmo(5, "grenade");
		CBasePlayer::GiveAmmo(5, "slam");

		GiveNamedItem("weapon_crowbar");
		GiveNamedItem("weapon_stunstick");
		GiveNamedItem("weapon_pistol");
		GiveNamedItem("weapon_357");

		GiveNamedItem("weapon_smg1");
		GiveNamedItem("weapon_ar2");

		GiveNamedItem("weapon_shotgun");
		GiveNamedItem("weapon_frag");

		GiveNamedItem("weapon_crossbow");

		GiveNamedItem("weapon_rpg");

		GiveNamedItem("weapon_slam");

		GiveNamedItem("weapon_physcannon");
	}
}

KeyValues* LoadWeaponConfig(const char* fileName)
{
	KeyValues* pConfig = new KeyValues("Spawn");

	char filePath[MAX_PATH];
	Q_snprintf(filePath, sizeof(filePath), "cfg/%s", fileName);

	// Try to load the file
	if (!pConfig->LoadFromFile(filesystem, filePath, "GAME"))
	{
		Msg("Failed to load weapon config file: %s\n", filePath);
		pConfig->deleteThis();
		return nullptr;
	}

	return pConfig;
}


void CHL2MP_Player::GiveDefaultItems( void )
{
	if (GetTeamNumber() != TEAM_SPECTATOR)
		EquipSuit();

	if (mp_spawnweapons.GetBool())
	{
		KeyValues* pSpawnConfig = LoadWeaponConfig("weapon_spawns.txt");

		if (pSpawnConfig)
		{
			KeyValues* pWeaponSection = pSpawnConfig->FindKey("Weapons");
			KeyValues* pAmmoSection = pSpawnConfig->FindKey("Ammo");

			if (pWeaponSection && pAmmoSection)
			{
				for (KeyValues* sub = pWeaponSection->GetFirstSubKey(); sub; sub = sub->GetNextKey())
				{
					const char* weaponName = sub->GetName();
					int shouldSpawn = atoi(sub->GetString());

					if (shouldSpawn > 0)
					{
						GiveNamedItem(UTIL_VarArgs("weapon_%s", weaponName));

						int ammoCount = atoi(pAmmoSection->GetString(weaponName, "0")); // Default to 0 if not found

						if (ammoCount > 0)
						{
							if (FStrEq(weaponName, "Pistol"))
								CBasePlayer::GiveAmmo(ammoCount, "Pistol");
							else if (FStrEq(weaponName, "357"))
								CBasePlayer::GiveAmmo(ammoCount, "357");
							else if (FStrEq(weaponName, "SMG1"))
								CBasePlayer::GiveAmmo(ammoCount, "SMG1");
							else if (FStrEq(weaponName, "AR2"))
								CBasePlayer::GiveAmmo(ammoCount, "AR2");
							else if (FStrEq(weaponName, "Shotgun"))
								CBasePlayer::GiveAmmo(ammoCount, "Buckshot");
							else if (FStrEq(weaponName, "Crossbow"))
								CBasePlayer::GiveAmmo(ammoCount, "XBowBolt");
							else if (FStrEq(weaponName, "Frag"))
								CBasePlayer::GiveAmmo(ammoCount, "grenade");
						}
					}
				}
			}

			pSpawnConfig->deleteThis();
		}
	}

	if (!mp_noweapons.GetBool() && GetTeamNumber() != TEAM_SPECTATOR)
	{
		CBasePlayer::GiveAmmo(255, "Pistol");
		CBasePlayer::GiveAmmo(45, "SMG1");
		CBasePlayer::GiveAmmo(1, "grenade");
		CBasePlayer::GiveAmmo(6, "Buckshot");
		CBasePlayer::GiveAmmo(6, "357");

		if (GetPlayerModelType() == PLAYER_SOUNDS_METROPOLICE || GetPlayerModelType() == PLAYER_SOUNDS_COMBINESOLDIER)
		{
			GiveNamedItem("weapon_stunstick");
		}
		else if (GetPlayerModelType() == PLAYER_SOUNDS_CITIZEN)
		{
			GiveNamedItem("weapon_crowbar");
		}

		GiveNamedItem("weapon_pistol");
		GiveNamedItem("weapon_smg1");
		GiveNamedItem("weapon_frag");
		GiveNamedItem("weapon_physcannon");

		const char *szDefaultWeaponName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_defaultweapon");

		CBaseCombatWeapon *pDefaultWeapon = Weapon_OwnsThisType(szDefaultWeaponName);

		if (pDefaultWeapon)
		{
			Weapon_Switch(pDefaultWeapon);
		}
		else
		{
			Weapon_Switch(Weapon_OwnsThisType("weapon_physcannon"));
		}
	}
}

void CHL2MP_Player::PickDefaultSpawnTeam( void )
{
	if ( GetTeamNumber() == 0 )
	{
		if ( HL2MPRules()->IsTeamplay() == false )
		{
			if ( GetModelPtr() == NULL )
			{
				const char *szModelName = NULL;
				szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

				if ( ValidatePlayerModel( szModelName ) == false )
				{
					char szReturnString[512];

					Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel models/combine_soldier.mdl\n" );
					engine->ClientCommand ( edict(), szReturnString );
				}

				ChangeTeam( TEAM_UNASSIGNED );
			}
		}
		else
		{
			CTeam *pCombine = g_Teams[TEAM_COMBINE];
			CTeam *pRebels = g_Teams[TEAM_REBELS];

			if ( pCombine == NULL || pRebels == NULL )
			{
				ChangeTeam( random->RandomInt( TEAM_COMBINE, TEAM_REBELS ) );
			}
			else
			{
				if ( pCombine->GetNumPlayers() > pRebels->GetNumPlayers() )
				{
					ChangeTeam( TEAM_REBELS );
				}
				else if ( pCombine->GetNumPlayers() < pRebels->GetNumPlayers() )
				{
					ChangeTeam( TEAM_COMBINE );
				}
				else
				{
					ChangeTeam( random->RandomInt( TEAM_COMBINE, TEAM_REBELS ) );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets HL2 specific defaults.
//-----------------------------------------------------------------------------
void CHL2MP_Player::Spawn(void)
{
	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	PickDefaultSpawnTeam();

	BaseClass::Spawn();

	SetNextThink(gpGlobals->curtime + 0.1f);
	SetThink(&CHL2MP_Player::FirstThinkAfterSpawn);

	if (mp_spawnprotection.GetBool())
		EnableSpawnProtection();

	SuitPower_RemoveDevice(SuitDeviceFlashlight);
	
	if ( !IsObserver() )
	{
		pl.deadflag = false;
		RemoveSolidFlags( FSOLID_NOT_SOLID );

		RemoveEffects( EF_NODRAW );
		
		SetAllowPickupWeaponThroughObstacle( true );
		GiveDefaultItems();
		SetAllowPickupWeaponThroughObstacle( false );
	}

	SetNumAnimOverlays( 3 );
	ResetAnimation();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;
	
	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_impactEnergyScale = HL2MPPLAYER_PHYSDAMAGE_SCALE;

	if ( HL2MPRules()->IsIntermission() )
	{
		AddFlag( FL_FROZEN );
	}
	else
	{
		RemoveFlag( FL_FROZEN );
	}

	SetSuitUpdate(NULL, false, 0); // stop ALL hev stuff

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);
}

void CHL2MP_Player::PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize )
{
	
}

bool CHL2MP_Player::ValidatePlayerModel( const char *pModel )
{
	int iModels = ARRAYSIZE( g_ppszRandomCitizenModels );
	int i;	

	for ( i = 0; i < iModels; ++i )
	{
		if ( !Q_stricmp( g_ppszRandomCitizenModels[i], pModel ) )
		{
			return true;
		}
	}

	iModels = ARRAYSIZE( g_ppszRandomCombineModels );

	for ( i = 0; i < iModels; ++i )
	{
	   	if ( !Q_stricmp( g_ppszRandomCombineModels[i], pModel ) )
		{
			return true;
		}
	}

	return false;
}

void CHL2MP_Player::SetPlayerTeamModel( void )
{
	const char *szModelName = NULL;
	szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

	int modelIndex = modelinfo->GetModelIndex( szModelName );

	if ( modelIndex == -1 || ValidatePlayerModel( szModelName ) == false )
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", szModelName );
		engine->ClientCommand ( edict(), szReturnString );
	}

	if ( GetTeamNumber() == TEAM_COMBINE )
	{
		if ( Q_stristr( szModelName, "models/human") )
		{
			int nHeads = ARRAYSIZE( g_ppszRandomCombineModels );
		
			g_iLastCombineModel = ( g_iLastCombineModel + 1 ) % nHeads;
			szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];
		}

		m_iModelType = TEAM_COMBINE;
	}
	else if ( GetTeamNumber() == TEAM_REBELS )
	{
		if ( !Q_stristr( szModelName, "models/human") )
		{
			int nHeads = ARRAYSIZE( g_ppszRandomCitizenModels );

			g_iLastCitizenModel = ( g_iLastCitizenModel + 1 ) % nHeads;
			szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];
		}

		m_iModelType = TEAM_REBELS;
	}
	
	SetModel( szModelName );
	SetupPlayerSoundsByModel( szModelName );

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;

	char szModelFileName[MAX_PATH];
	V_FileBase(szModelName, szModelFileName, sizeof(szModelFileName));

	// Print the model name to the client
	if (GetTeamNumber() != TEAM_SPECTATOR)
		UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "Your player model is: " CHAT_INFO "%s\n", szModelFileName));
}

void CHL2MP_Player::SetPlayerModel( void )
{
	const char *szModelName = NULL;
	const char *pszCurrentModelName = modelinfo->GetModelName( GetModel());

	szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

	if ( ValidatePlayerModel( szModelName ) == false )
	{
		char szReturnString[512];

		if ( ValidatePlayerModel( pszCurrentModelName ) == false )
		{
			pszCurrentModelName = "models/Combine_Soldier.mdl";
		}

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", pszCurrentModelName );
		engine->ClientCommand ( edict(), szReturnString );

		szModelName = pszCurrentModelName;
	}

	if ( GetTeamNumber() == TEAM_COMBINE )
	{
		int nHeads = ARRAYSIZE( g_ppszRandomCombineModels );
		
		g_iLastCombineModel = ( g_iLastCombineModel + 1 ) % nHeads;
		szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];

		m_iModelType = TEAM_COMBINE;
	}
	else if ( GetTeamNumber() == TEAM_REBELS )
	{
		int nHeads = ARRAYSIZE( g_ppszRandomCitizenModels );

		g_iLastCitizenModel = ( g_iLastCitizenModel + 1 ) % nHeads;
		szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];

		m_iModelType = TEAM_REBELS;
	}
	else
	{
		if ( Q_strlen( szModelName ) == 0 ) 
		{
			szModelName = g_ppszRandomCitizenModels[0];
		}

		if ( Q_stristr( szModelName, "models/human") )
		{
			m_iModelType = TEAM_REBELS;
		}
		else
		{
			m_iModelType = TEAM_COMBINE;
		}
	}

	int modelIndex = modelinfo->GetModelIndex( szModelName );

	if ( modelIndex == -1 )
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", szModelName );
		engine->ClientCommand ( edict(), szReturnString );
	}

	SetModel( szModelName );
	SetupPlayerSoundsByModel( szModelName );

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetupPlayerSoundsByModel( const char *pModelName )
{
	if ( Q_stristr( pModelName, "models/human") )
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
	}
	else if ( Q_stristr(pModelName, "police" ) )
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_METROPOLICE;
	}
	else if ( Q_stristr(pModelName, "combine" ) )
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_COMBINESOLDIER;
	}
}

void CHL2MP_Player::ResetAnimation( void )
{
	if ( IsAlive() )
	{
		SetSequence ( -1 );
		SetActivity( ACT_INVALID );

		if ((GetAbsVelocity().x || GetAbsVelocity().y) && (GetFlags() & FL_ONGROUND) || GetWaterLevel() > 1)
		{
			SetAnimation(PLAYER_WALK);
		}
		else
		{
			SetAnimation(PLAYER_IDLE);
		}
	}
}


bool CHL2MP_Player::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	bool bRet = BaseClass::Weapon_Switch( pWeapon, viewmodelindex );

	if ( bRet == true )
	{
		ResetAnimation();
	}

	return bRet;
}

void CHL2MP_Player::PreThink( void )
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if ( vTempAngles[PITCH] > 180.0f )
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();
	State_PreThink();

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;
	SetLocalAngles( vOldAngles );
}

bool CHL2MP_Player::IsReady()
{
	return m_bReady;
}

void CHL2MP_Player::SetReady(bool bReady)
{
	m_bReady = bReady;
}

bool CHL2MP_Player::SavePlayerSettings()
{
	if (IsBot())
		return false;

	const char* steamID3 = engine->GetPlayerNetworkIDString(edict());  // Fetch SteamID3
	uint64 steamID64 = ConvertSteamID3ToSteamID64(steamID3);  // Convert to SteamID64

	char filename[MAX_PATH];
	Q_snprintf(filename, sizeof(filename), "cfg/core/%llu.txt", steamID64);  // Use SteamID64 for filename

	// Ensure the directory exists
	if (!filesystem->FileExists("cfg/core", "GAME"))
	{
		filesystem->CreateDirHierarchy("cfg/core", "GAME");
	}

	KeyValues* kv = new KeyValues("Settings");

	// Load existing settings if the file exists
	kv->LoadFromFile(filesystem, filename, "MOD");

	KeyValues* playerSettings = kv->FindKey(UTIL_VarArgs("%llu", steamID64), true);
	playerSettings->SetInt("FOV", m_iFOV);
	playerSettings->SetInt("FOVServer", m_iFOVServer);
	playerSettings->SetInt(".357 Zoom Level", Get357ZoomLevel());
	playerSettings->SetInt("Xbow Zoom Level", GetXbowZoomLevel());

	// Save hit sound and kill sound settings using getters
	playerSettings->SetInt("HitSoundsEnabled", AreHitSoundsEnabled() ? 1 : 0);
	playerSettings->SetInt("KillSoundsEnabled", AreKillSoundsEnabled() ? 1 : 0);

	if (kv->SaveToFile(filesystem, filename, "MOD"))
	{
		Msg("Player settings saved successfully in cfg/core/.\n");
	}
	else
	{
		Warning("Failed to save player settings in cfg/core/.\n");
	}

	kv->deleteThis();
	return true;
}

bool CHL2MP_Player::LoadPlayerSettings()
{
	if (IsBot())
		return false;

	const char* steamID3 = engine->GetPlayerNetworkIDString(edict());  // Fetch SteamID3
	uint64 steamID64 = ConvertSteamID3ToSteamID64(steamID3);  // Convert to SteamID64

	char filename[MAX_PATH];
	Q_snprintf(filename, sizeof(filename), "cfg/core/%llu.txt", steamID64);  // Use SteamID64 for filename

	KeyValues* kv = new KeyValues("Settings");

	if (!kv->LoadFromFile(filesystem, filename, "MOD"))
	{
		Warning("Couldn't load settings from file %s, creating a new one with default values...\n", filename);

		// Set default values
		m_iFOV = 90;
		m_iFOVServer = 90;
		Set357ZoomLevel(20);  // Default .357 zoom level
		SetXbowZoomLevel(20);  // Default crossbow zoom level
		SetHitSoundsEnabled(true);  // Enable hit sounds by default
		SetKillSoundsEnabled(true);  // Enable kill sounds by default

		// Create the player settings KeyValues entry
		KeyValues* playerSettings = new KeyValues(UTIL_VarArgs("%llu", steamID64));
		playerSettings->SetInt("FOV", m_iFOV);
		playerSettings->SetInt("FOVServer", m_iFOVServer);
		playerSettings->SetInt(".357 Zoom Level", Get357ZoomLevel());
		playerSettings->SetInt("Xbow Zoom Level", GetXbowZoomLevel());
		playerSettings->SetBool("HitSoundsEnabled", AreHitSoundsEnabled());
		playerSettings->SetBool("KillSoundsEnabled", AreKillSoundsEnabled());

		// Add player settings to the main KeyValues
		kv->AddSubKey(playerSettings);

		// Save the new settings to the file
		if (kv->SaveToFile(filesystem, filename, "MOD"))
		{
			Msg("Default settings for player %s have been saved to %s\n", steamID3, filename);
		}
		else
		{
			Warning("Failed to save default settings to file %s\n", filename);
			kv->deleteThis();
			return false;
		}

		kv->deleteThis();
		return true;
	}

	KeyValues* playerSettings = kv->FindKey(UTIL_VarArgs("%llu", steamID64));
	if (!playerSettings)
	{
		Warning("Player settings not found in file %s\n", filename);
		kv->deleteThis();
		return false;
	}

	m_iFOV = playerSettings->GetInt("FOV", m_iFOV);
	m_iFOVServer = playerSettings->GetInt("FOVServer", m_iFOVServer);
	Set357ZoomLevel(playerSettings->GetInt(".357 Zoom Level", Get357ZoomLevel()));
	SetXbowZoomLevel(playerSettings->GetInt("Xbow Zoom Level", GetXbowZoomLevel()));

	// Load hit sound and kill sound settings using GetInt(), with default values of 1 (enabled)
	SetHitSoundsEnabled(playerSettings->GetBool("HitSoundsEnabled", AreHitSoundsEnabled()));  // Default to enabled
	SetKillSoundsEnabled(playerSettings->GetBool("KillSoundsEnabled", AreKillSoundsEnabled())); // Default to enabled

	kv->deleteThis();
	return true;
}

void CHL2MP_Player::CheckChatText(char* p, int bufsize)
{
	//Look for escape sequences and replace

	char* buf = new char[bufsize];
	int pos = 0;

	// Parse say text for escape sequences
	for (char* pSrc = p; pSrc != NULL && *pSrc != 0 && pos < bufsize - 1; pSrc++)
	{
		// copy each char across
		buf[pos] = *pSrc;
		pos++;
	}

	buf[pos] = '\0';

	// copy buf back into p
	Q_strncpy(p, buf, bufsize);

	delete[] buf;

	const char* pReadyCheck = p;

	if (Q_strncmp(p, "!fov", strlen("!fov")) == 0)
	{
		const char* argStart = strstr(p, "!fov");

		if ((IsBot()))
			return;

		if (argStart)
		{
			argStart += Q_strlen("!fov");
			while (*argStart == ' ')
			{
				argStart++;
			}

			if (*argStart != '\0')
			{
				int iFovValue = atoi(argStart);

				if (iFovValue < 70 || iFovValue > 110)
				{
					UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "FOV can only be set between " CHAT_WHITE "70 " CHAT_CONTEXT "and " CHAT_WHITE "110"));
					return;
				}

				if (GetFOV() == iFovValue)
				{
					UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "FOV is already set to " CHAT_WHITE "%d", GetFOV()));
					return;
				}

				char sFovValue[64];
				Q_snprintf(sFovValue, sizeof(sFovValue), CHAT_CONTEXT "FOV is now set to " CHAT_WHITE "%d", iFovValue);
				UTIL_PrintToClient(this, sFovValue);
				iFovValue = clamp(iFovValue, 70, 110);
				m_iFOVServer = iFovValue;
				m_iFOV = iFovValue;
				SetDefaultFOV(m_iFOV);
				SavePlayerSettings();  // Save settings after changing the FOV
			}
			else
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "FOV is " CHAT_WHITE "%d", GetFOV()));
			}
		}
	}

	if (Q_strncmp(p, "!mzl", strlen("!mzl")) == 0)
	{
		const char* argStart = strstr(p, "!mzl");

		if (IsBot())
			return;

		if (argStart)
		{
			argStart += Q_strlen("!mzl");
			while (*argStart == ' ')
			{
				argStart++;
			}

			// If no argument is provided, print the current value
			if (*argStart == '\0')
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "Your .357 zoom level: " CHAT_WHITE "%d", Get357ZoomLevel()));
				return;
			}

			// If an argument is provided, set the zoom level
			int zoomLevel = atoi(argStart);

			if (zoomLevel < 20 || zoomLevel > 40)
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT ".357 zoom level can only be set between " CHAT_WHITE "20 " CHAT_CONTEXT "and " CHAT_WHITE "40"));
				return;
			}

			if (Get357ZoomLevel() == zoomLevel)
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT ".357 zoom level is already set to " CHAT_WHITE "%d", zoomLevel));
				return;
			}


			if (IsWeaponZoomActive())
			{
				UTIL_PrintToClient(this, CHAT_CONTEXT "You cannot change your zoom level while zoomed in");
				return;
			}

			Set357ZoomLevel(zoomLevel);
			SavePlayerSettings();  // Save the new zoom level to the file
			UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT ".357 zoom level set to " CHAT_WHITE "%d", zoomLevel));
		}
	}

	if (Q_strncmp(p, "!czl", strlen("!czl")) == 0)
	{
		const char* argStart = strstr(p, "!czl");

		if (IsBot())
			return;

		if (argStart)
		{
			argStart += Q_strlen("!czl");
			while (*argStart == ' ')
			{
				argStart++;
			}

			// If no argument is provided, print the current value
			if (*argStart == '\0')
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "Crossbow zoom level: " CHAT_WHITE "%d", GetXbowZoomLevel()));
				return;
			}

			// If an argument is provided, set the zoom level
			int zoomLevel = atoi(argStart);

			if (zoomLevel < 20 || zoomLevel > 40)
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "Crossbow zoom level can only be set between " CHAT_WHITE "20 " CHAT_CONTEXT "and " CHAT_WHITE "40"));
				return;
			}

			if (GetXbowZoomLevel() == zoomLevel)
			{
				UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "Crossbow zoom level is already set to " CHAT_WHITE "%d", zoomLevel));
				return;
			}

			if (IsWeaponZoomActive())
			{
				UTIL_PrintToClient(this, CHAT_CONTEXT "You cannot change your zoom level while zoomed in");
				return;
			}

			SetXbowZoomLevel(zoomLevel);
			SavePlayerSettings();  // Save the new zoom level to the file
			UTIL_PrintToClient(this, UTIL_VarArgs(CHAT_CONTEXT "Crossbow zoom level set to " CHAT_WHITE "%d", zoomLevel));
		}
	}

	if (Q_stricmp(p, "!hs") == 0)
	{
		bool newHitSoundState = !AreHitSoundsEnabled();
		SetHitSoundsEnabled(newHitSoundState);
		if (newHitSoundState)
		{
			UTIL_PrintToClient(this, CHAT_CONTEXT "Hit sounds " CHAT_WHITE "enabled");
		}
		else
		{
			UTIL_PrintToClient(this, CHAT_CONTEXT "Hit sounds " CHAT_WHITE "disabled");
		}
		SavePlayerSettings();
		return;
	}

	// Command to toggle kill sounds
	if (Q_stricmp(p, "!ks") == 0)
	{
		bool newKillSoundState = !AreKillSoundsEnabled();
		SetKillSoundsEnabled(newKillSoundState);
		if (newKillSoundState)
		{
			UTIL_PrintToClient(this, CHAT_CONTEXT "Kill sounds " CHAT_WHITE "enabled");
		}
		else
		{
			UTIL_PrintToClient(this, CHAT_CONTEXT "Kill sounds " CHAT_WHITE "disabled");
		}

		SavePlayerSettings();
		return;
	}

	if (Q_stricmp(p, "!teams") == 0 && sv_teamsmenu.GetBool())
	{
		if (serverpluginhelpers)
		{
			KeyValues* kv = new KeyValues("menu");

			kv->SetString("title", "Team Selection");
			kv->SetInt("level", 1);
			kv->SetColor("color", Color(255, 255, 255, 255));
			kv->SetInt("time", 20);
			kv->SetString("msg", "Choose a team or hit ESC to exit");

			KeyValues* item1 = kv->FindKey("1", true);
			item1->SetString("msg", "Spectate");
			item1->SetString("command", "spectate; play buttons/combine_button1.wav");

			KeyValues* item2 = kv->FindKey("2", true);
			item2->SetString("msg", "Combine");
			item2->SetString("command", "jointeam 2; play buttons/combine_button1.wav");

			KeyValues* item3 = kv->FindKey("3", true);
			item3->SetString("msg", "Rebels");
			item3->SetString("command", "jointeam 3; play buttons/combine_button1.wav");

			serverpluginhelpers->CreateMessage(this->edict(), DIALOG_MENU, kv, &g_EmptyPluginCallbacks);

			kv->deleteThis();
		}
		return;
	}

	return;

	HL2MPRules()->CheckChatForReadySignal(this, pReadyCheck);
}

void FOVConsoleCommand(const CCommand& args);

ConCommand fov("fov", FOVConsoleCommand, "Change player FOV via console. Usage: fov <value>", FCVAR_CLIENTCMD_CAN_EXECUTE);

void FOVConsoleCommand(const CCommand& args)
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());

	if (pPlayer == NULL || args.ArgC() < 2)  // Make sure a valid argument is passed (FOV value)
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs("\"fov\" is \"%d\"\nUsage: fov <value>", pPlayer->GetFOV()));
		return;
	}

	// Block bots from using the command
	if (pPlayer->IsBot())
	{
		return;
	}

	int iFovValue = atoi(args[1]);  // Get the FOV value from console command argument

	if (iFovValue < 70 || iFovValue > 110)
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, "Your FOV can only be set between 70 and 110.\n");
		return;
	}

	if (pPlayer->GetFOV() == iFovValue)
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs("Your FOV is already set to %d.\n", pPlayer->GetFOV()));
		return;
	}

	pPlayer->SetFOVServer(iFovValue);  // Set the server FOV value
	pPlayer->SetFOV(iFovValue);        // Update the FOV
	pPlayer->SetDefaultFOV(iFovValue); // Update the player's default FOV

	// Save player settings to the file
	pPlayer->SavePlayerSettings();

	// Inform the player via console
	ClientPrint(pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs("Your FOV is now set to %d.\n", iFovValue));
}

CON_COMMAND(mzl, "Set or check .357 zoom level")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	if (args.ArgC() == 1) // No argument, print the current zoom level
	{
		char command[8];  // Create a writable buffer
		Q_strncpy(command, "!mzl", sizeof(command));  // Copy the string into the buffer
		pPlayer->CheckChatText(command, sizeof(command));  // Pass the writable buffer to CheckChatText
	}
	else if (args.ArgC() == 2) // Argument provided, set the zoom level
	{
		char command[16];
		Q_snprintf(command, sizeof(command), "!mzl %s", args[1]);  // Ensure dynamic sizing
		pPlayer->CheckChatText(command, sizeof(command));
	}
}

CON_COMMAND(czl, "Set or check crossbow zoom level")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	if (args.ArgC() == 1) // No argument, print the current zoom level
	{
		char command[8];  // Create a writable buffer
		Q_strncpy(command, "!czl", sizeof(command));  // Copy the string into the buffer
		pPlayer->CheckChatText(command, sizeof(command));  // Pass the writable buffer to CheckChatText
	}
	else if (args.ArgC() == 2) // Argument provided, set the zoom level
	{
		char command[16];
		Q_snprintf(command, sizeof(command), "!czl %s", args[1]);  // Ensure dynamic sizing
		pPlayer->CheckChatText(command, sizeof(command));
	}
}

void CHL2MP_Player::PostThink( void )
{
	BaseClass::PostThink();
	
	if ( GetFlags() & FL_DUCKING )
	{
		SetCollisionBounds( VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX );
	}

	m_PlayerAnimState.Update();

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
}

void CHL2MP_Player::PlayerDeathThink()
{
	if( !IsObserver() )
	{
		BaseClass::PlayerDeathThink();
	}
}

void CHL2MP_Player::FireBullets ( const FireBulletsInfo_t &info )
{
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation(this, this->GetCurrentCommand());

	FireBulletsInfo_t modinfo = info;

	CWeaponHL2MPBase *pWeapon = dynamic_cast<CWeaponHL2MPBase *>( GetActiveWeapon() );

	if ( pWeapon )
	{
		modinfo.m_iPlayerDamage = modinfo.m_flDamage = pWeapon->GetHL2MPWpnData().m_iPlayerDamage;
	}

	NoteWeaponFired();

	BaseClass::FireBullets( modinfo );

	// Move other players back to history positions based on local player's lag
	lagcompensation->FinishLagCompensation( this );
}

void CHL2MP_Player::NoteWeaponFired( void )
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CHL2MP_Player::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	//SHOTGUN SECONDARY ATTACK PREDICTION FIX
	if ( !( ( pCmd->buttons & IN_ATTACK ) || ( pCmd->buttons & IN_ATTACK2 ) ) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5) )
		return false;

	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if ( pEntityTransmitBits && !pEntityTransmitBits->Get( pPlayer->entindex() ) )
		return false;

	const Vector &vMyOrigin = GetAbsOrigin();
	const Vector &vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if ( vHisOrigin.DistTo( vMyOrigin ) < maxDistance )
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors( pCmd->viewangles, &vForward );
	
	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize( vDiff );

	float flCosAngle = 0.707107f;	// 45 degree angle
	if ( vForward.Dot( vDiff ) < flCosAngle )
		return false;

	return true;
}

Activity CHL2MP_Player::TranslateTeamActivity( Activity ActToTranslate )
{
	if ( m_iModelType == TEAM_COMBINE )
		 return ActToTranslate;
	
	if ( ActToTranslate == ACT_RUN )
		 return ACT_RUN_AIM_AGITATED;

	if ( ActToTranslate == ACT_IDLE )
		 return ACT_IDLE_AIM_AGITATED;

	if ( ActToTranslate == ACT_WALK )
		 return ACT_WALK_AIM_AGITATED;

	return ActToTranslate;
}

extern ConVar hl2_normspeed;

// Set the activity based on an event or current state
void CHL2MP_Player::SetAnimation( PLAYER_ANIM playerAnim )
{
	int animDesired;

	float speed;

	speed = GetAbsVelocity().Length2D();

	
	// bool bRunning = true;

	//Revisit!
/*	if ( ( m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) )
	{
		if ( speed > 1.0f && speed < hl2_normspeed.GetFloat() - 20.0f )
		{
			bRunning = false;
		}
	}*/

	if ( GetFlags() & ( FL_FROZEN | FL_ATCONTROLS ) )
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	Activity idealActivity = ACT_HL2MP_RUN;

	// This could stand to be redone. Why is playerAnim abstracted from activity? (sjb)
	if ( playerAnim == PLAYER_JUMP )
	{
		idealActivity = ACT_HL2MP_JUMP;
	}
	else if ( playerAnim == PLAYER_DIE )
	{
		if ( m_lifeState == LIFE_ALIVE )
		{
			return;
		}
	}
	else if ( playerAnim == PLAYER_ATTACK1 )
	{
		if ( GetActivity( ) == ACT_HOVER	|| 
			 GetActivity( ) == ACT_SWIM		||
			 GetActivity( ) == ACT_HOP		||
			 GetActivity( ) == ACT_LEAP		||
			 GetActivity( ) == ACT_DIESIMPLE )
		{
			idealActivity = GetActivity( );
		}
		else
		{
			idealActivity = ACT_HL2MP_GESTURE_RANGE_ATTACK;
		}
	}
	else if ( playerAnim == PLAYER_RELOAD )
	{
		idealActivity = ACT_HL2MP_GESTURE_RELOAD;
	}
	else if ( playerAnim == PLAYER_IDLE || playerAnim == PLAYER_WALK )
	{
		if ( !( GetFlags() & FL_ONGROUND ) && GetActivity( ) == ACT_HL2MP_JUMP )	// Still jumping
		{
			idealActivity = GetActivity( );
		}
		/*
		else if ( GetWaterLevel() > 1 )
		{
			if ( speed == 0 )
				idealActivity = ACT_HOVER;
			else
				idealActivity = ACT_SWIM;
		}
		*/
		else
		{
			if ( GetFlags() & FL_ANIMDUCKING )
			{
				if ( speed > 0 )
				{
					idealActivity = ACT_HL2MP_WALK_CROUCH;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE_CROUCH;
				}
			}
			else
			{
				if ( speed > 0 )
				{
					/*
					if ( bRunning == false )
					{
						idealActivity = ACT_WALK;
					}
					else
					*/
					{
						idealActivity = ACT_HL2MP_RUN;
					}
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE;
				}
			}
		}

		idealActivity = TranslateTeamActivity( idealActivity );
	}
	
	if ( idealActivity == ACT_HL2MP_GESTURE_RANGE_ATTACK )
	{
		RestartGesture( Weapon_TranslateActivity( idealActivity ) );

		// FIXME: this seems a bit wacked
		Weapon_SetActivity( Weapon_TranslateActivity( ACT_RANGE_ATTACK1 ), 0 );

		return;
	}
	else if ( idealActivity == ACT_HL2MP_GESTURE_RELOAD )
	{
		RestartGesture( Weapon_TranslateActivity( idealActivity ) );
		return;
	}
	else
	{
		SetActivity( idealActivity );

		animDesired = SelectWeightedSequence( Weapon_TranslateActivity ( idealActivity ) );

		if (animDesired == -1)
		{
			animDesired = SelectWeightedSequence( idealActivity );

			if ( animDesired == -1 )
			{
				animDesired = 0;
			}
		}
	
		// Already using the desired animation?
		if ( GetSequence() == animDesired )
			return;

		m_flPlaybackRate = 1.0;
		ResetSequence( animDesired );
		SetCycle( 0 );
		return;
	}

	// Already using the desired animation?
	if ( GetSequence() == animDesired )
		return;

	//Msg( "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	ResetSequence( animDesired );
	SetCycle( 0 );
}


extern int	gEvilImpulse101;
//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CHL2MP_Player::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( !IsAllowedToPickupWeapons() )
		return false;

	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	if( !GetAllowPickupWeaponThroughObstacle() )
	{ 
		// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
		if( !pWeapon->FVisible( this, MASK_SOLID ) && !(GetFlags() & FL_NOTARGET) )
		{
			return false;
		}
	}

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType());

	if ( bOwnsWeaponAlready == true ) 
	{
		//If we have room for the ammo, then "take" the weapon too.
		 if ( Weapon_EquipAmmoOnly( pWeapon ) )
		 {
			 pWeapon->CheckRespawn();

			 UTIL_Remove( pWeapon );
			 return true;
		 }
		 else
		 {
			 return false;
		 }
	}

	pWeapon->CheckRespawn();
	Weapon_Equip( pWeapon );

	return true;
}

void CHL2MP_Player::ChangeTeam( int iTeam )
{
	LadderRespawnFix();	

	CHL2MPRules* pRules = HL2MPRules();
	if (pRules)
	{
		pRules->ResetKillStreaks(this);
	}

	// bool bKill = false;
	bool bWasSpectator = false;

	if ( HL2MPRules()->IsTeamplay() != true && iTeam != TEAM_SPECTATOR )
	{
		//don't let them try to join combine or rebels during deathmatch.
		iTeam = TEAM_UNASSIGNED;
	}

	if (this->GetTeamNumber() == TEAM_SPECTATOR)
	{
		bWasSpectator = true;
	}

	BaseClass::ChangeTeam( iTeam );

	m_flNextTeamChangeTime = gpGlobals->curtime + TEAM_CHANGE_INTERVAL;

	if ( HL2MPRules()->IsTeamplay() == true )
	{
		SetPlayerTeamModel();

		// Little bit dirty, but this will work for now.
		if (iTeam == TEAM_REBELS)
		{
			engine->ClientCommand(edict(), "cl_playermodel models/humans/group03/female_04.mdl\n");
		}
		else if (iTeam == TEAM_COMBINE)
		{
			engine->ClientCommand(edict(), "cl_playermodel models/combine_soldier.mdl\n");
		}
	}
	else
	{
		SetPlayerModel();
	}

	if (bWasSpectator)
	{
		Spawn();
		return; // everything is useless afterwards
	}

	DetonateTripmines();
	ClearUseEntity();

	if ( iTeam == TEAM_SPECTATOR )
	{
		// Thanks No Air and Adrian for this!
		ForceDropOfCarriedPhysObjects(NULL);

		StopZooming();

		// Fixes the sprinting issue and suit zoom as spec
		RemoveAllItems( true );

		if (FlashlightIsOn())
		{
			FlashlightTurnOff();
		}

		if (IsInAVehicle())
		{
			LeaveVehicle();
		}

		State_Transition( STATE_OBSERVER_MODE );
	}
}

bool CHL2MP_Player::HandleCommand_JoinTeam(int team)
{
	if (team == TEAM_SPECTATOR && IsHLTV())
	{
		ChangeTeam(TEAM_SPECTATOR);
		ResetDeathCount();
		ResetFragCount();
		return true;
	}

	if (!GetGlobalTeam(team) || team == 0)
	{
		char szReturnString[128];
		Q_snprintf(szReturnString, sizeof(szReturnString), "Please enter a valid team index.\n");
		ClientPrint(this, HUD_PRINTTALK, szReturnString);
		return false;
	}

	// Don't do anything if you join your own team
	if (team == GetTeamNumber())
	{
		return false;
	}

	// end early
	if (GetTeamNumber() == TEAM_SPECTATOR && !sv_lockteams.GetBool())
	{
		if (sv_lockteams.GetBool())
		{
			UTIL_PrintToClient(this, CHAT_RED "Teams are currently locked!\n");
			return true;
		}

		ChangeTeam(team);
		return true;
	}

	if (team == TEAM_SPECTATOR)
	{
		// Prevent this is the cvar is set
		if (!mp_allowspectators.GetInt())
		{
			ClientPrint(this, HUD_PRINTCENTER, "#Cannot_Be_Spectator");
			return false;
		}

		if (sv_lockteams.GetBool())
		{
			UTIL_PrintToClient(this, CHAT_RED "Teams are currently locked!\n");
			return true;
		}

		ChangeTeam(TEAM_SPECTATOR);

		return true;
	}
	else
	{
		if (sv_lockteams.GetBool())
		{
			UTIL_PrintToClient(this, CHAT_RED "Teams are currently locked!\n");
			return true;
		}

		StopObserverMode();
		State_Transition(STATE_ACTIVE);
	}

	// Switch their actual team...
	ChangeTeam(team);

	return true;
}

bool CHL2MP_Player::ClientCommand( const CCommand &args )
{
	if ( FStrEq( args[0], "spectate" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			// instantly join spectators
			HandleCommand_JoinTeam( TEAM_SPECTATOR );	
		}

		return true;
	}
	else if ( FStrEq( args[0], "jointeam" ) ) 
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			int iTeam = atoi( args[1] );
			HandleCommand_JoinTeam( iTeam );
		}
		return true;
	}
	else if ( FStrEq( args[0], "joingame" ) )
	{
		if (IsObserver())
			this->ChangeTeam(random->RandomInt(2, 3));
		return true;
	}

	return BaseClass::ClientCommand( args );
}

void CHL2MP_Player::CheatImpulseCommands( int iImpulse )
{
	switch ( iImpulse )
	{
		case 101:
			{
				if( sv_cheats->GetBool() && GetTeamNumber() != TEAM_SPECTATOR )
				{
					GiveAllItems();
				}
			}
			break;

		default:
			BaseClass::CheatImpulseCommands( iImpulse );
	}
}

bool CHL2MP_Player::ShouldRunRateLimitedCommand( const CCommand &args )
{
	int i = m_RateLimitLastCommandTimes.Find( args[0] );
	if ( i == m_RateLimitLastCommandTimes.InvalidIndex() )
	{
		m_RateLimitLastCommandTimes.Insert( args[0], gpGlobals->curtime );
		return true;
	}
	else if ( (gpGlobals->curtime - m_RateLimitLastCommandTimes[i]) < HL2MP_COMMAND_MAX_RATE )
	{
		// Too fast.
		return false;
	}
	else
	{
		m_RateLimitLastCommandTimes[i] = gpGlobals->curtime;
		return true;
	}
}

void CHL2MP_Player::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this, false );
		m_hViewModel.Set( index, vm );
	}
}

bool CHL2MP_Player::BecomeRagdollOnClient( const Vector &force )
{
	return true;
}

// -------------------------------------------------------------------------------- //
// Ragdoll entities.
// -------------------------------------------------------------------------------- //

class CHL2MPRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS( CHL2MPRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle 
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

LINK_ENTITY_TO_CLASS( hl2mp_ragdoll, CHL2MPRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CHL2MPRagdoll, DT_HL2MPRagdoll )
	SendPropVector( SENDINFO(m_vecRagdollOrigin), -1,  SPROP_COORD ),
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
	SendPropInt		( SENDINFO(m_nForceBone), 8, 0 ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ) )
END_SEND_TABLE()


void CHL2MP_Player::CreateRagdollEntity( void )
{
	if ( m_hRagdoll )
	{
		UTIL_RemoveImmediate( m_hRagdoll );
		m_hRagdoll = NULL;
	}

	// If we already have a ragdoll, don't make another one.
	CHL2MPRagdoll *pRagdoll = dynamic_cast< CHL2MPRagdoll* >( m_hRagdoll.Get() );
	
	if ( !pRagdoll )
	{
		// create a new one
		pRagdoll = dynamic_cast< CHL2MPRagdoll* >( CreateEntityByName( "hl2mp_ragdoll" ) );
	}

	if ( pRagdoll )
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->SetAbsOrigin( GetAbsOrigin() );
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2MP_Player::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOn( void )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
		EmitSound( "HL2Player.FlashlightOn" );
		if (!sv_infinite_flashlight.GetBool())
			SuitPower_AddDevice(SuitDeviceFlashlight);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2MP_Player::FlashlightTurnOff( void )
{
	RemoveEffects( EF_DIMLIGHT );
	
	if( IsAlive() )
	{
		EmitSound( "HL2Player.FlashlightOff" );
		SuitPower_RemoveDevice(SuitDeviceFlashlight);
	}
}

void CHL2MP_Player::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget, const Vector *pVelocity )
{
	//Drop a grenade if it's primed.
	if ( GetActiveWeapon() )
	{
		CBaseCombatWeapon *pGrenade = Weapon_OwnsThisType("weapon_frag");

		if ( GetActiveWeapon() == pGrenade )
		{
			if ( ( m_nButtons & IN_ATTACK ) || (m_nButtons & IN_ATTACK2) )
			{
				DropPrimedFragGrenade( this, pGrenade );
				return;
			}
		}
	}

	if (pWeapon && pWeapon->Clip1() <= 0 && !FClassnameIs(pWeapon, "weapon_rpg"))
	{
		// Msg("Weapon was out of ammo, removed\n");
		UTIL_Remove(pWeapon);
		return;
	}

	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );
}


void CHL2MP_Player::DetonateTripmines( void )
{
	CBaseEntity *pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "npc_satchel" )) != NULL)
	{
		CSatchelCharge *pSatchel = dynamic_cast<CSatchelCharge *>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() == this )
		{
			g_EventQueue.AddEvent( pSatchel, "Explode", 0.20, this, this );
		}
	}
}

void CHL2MP_Player::LadderRespawnFix()
{
	if ( auto lm = GetLadderMove() )
	{
		if ( lm->m_bForceLadderMove )
		{
			lm->m_bForceLadderMove = false;
			if ( lm->m_hReservedSpot )
			{
				UTIL_Remove((CBaseEntity*)lm->m_hReservedSpot.Get());
				lm->m_hReservedSpot = NULL;
			}
		}
	}
}

void CHL2MP_Player::Event_Killed( const CTakeDamageInfo &info )
{
	//Ladder respawn fix
	LadderRespawnFix();

	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce( m_vecTotalBulletForce );

	SetNumAnimOverlays( 0 );

	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity();

	DetonateTripmines();

	BaseClass::Event_Killed( subinfo );

	if ( info.GetDamageType() & DMG_DISSOLVE )
	{
		if ( m_hRagdoll )
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}
	}

	CBaseEntity *pAttacker = info.GetAttacker();

	if ( pAttacker )
	{
		int iScoreToAdd = 1;

		if ( pAttacker == this )
		{
			iScoreToAdd = -1;
		}

		GetGlobalTeam( pAttacker->GetTeamNumber() )->AddScore( iScoreToAdd );
	}

	FlashlightTurnOff();

	m_lifeState = LIFE_DEAD;

	RemoveEffects( EF_NODRAW );	// still draw player body
	StopZooming();
}

int CHL2MP_Player::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	if (m_bSpawnProtected)
	{
		return 0;  // Ignore the damage
	}

	//return here if the player is in the respawn grace period vs. slams.
	if ( gpGlobals->curtime < m_flSlamProtectTime &&  (inputInfo.GetDamageType() == DMG_BLAST ) )
		return 0;

	m_vecTotalBulletForce += inputInfo.GetDamageForce();
	
	gamestats->Event_PlayerDamage( this, inputInfo );

	return BaseClass::OnTakeDamage( inputInfo );
}

void CHL2MP_Player::DeathSound( const CTakeDamageInfo &info )
{
	if ( m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving() )
		 return;

	char szStepSound[128];

	Q_snprintf( szStepSound, sizeof( szStepSound ), "%s.Die", GetPlayerModelSoundPrefix() );

	const char *pModelName = STRING( GetModelName() );

	CSoundParameters params;
	if ( GetParametersForSound( szStepSound, params, pModelName ) == false )
		return;

	Vector vecOrigin = GetAbsOrigin();
	
	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );
}

ConVar sv_spawnpoint_lineofsight("sv_spawnpoint_lineofsight", "1", FCVAR_REPLICATED, "Check if spawn point has line of sight to another player");
ConVar sv_spawnradius("sv_spawnradius", "256", FCVAR_REPLICATED, "Radius around spawn point to avoid spawning near other players");

CBaseEntity* CHL2MP_Player::EntSelectSpawnPoint(void)
{
	CBaseEntity* pSpot = NULL;
	CBaseEntity* pLastSpawnPoint = g_pLastSpawn;
	const char* pSpawnpointName = "info_player_deathmatch";

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			pSpawnpointName = "info_player_combine";
			pLastSpawnPoint = g_pLastCombineSpawn;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			pSpawnpointName = "info_player_rebel";
			pLastSpawnPoint = g_pLastRebelSpawn;
		}

		if (gEntList.FindEntityByClassname(NULL, pSpawnpointName) == NULL)
		{
			pSpawnpointName = "info_player_deathmatch";
			pLastSpawnPoint = g_pLastSpawn;
		}
	}

	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for (int i = random->RandomInt(1, 5); i > 0; i--)
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	if (!pSpot)  // skip over the null point
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);

	CBaseEntity* pFirstSpot = pSpot;

	// What we're going to do is randomize the spawn points as Valve intended, 
	// but we're also going to attempt to limit spawnkilling by not spawning 
	// a player near an alive player if that spawn point has a direct line of sight 
	// or is within radius. This is done to prevent farming easy frag points. 
	// If both logic fail, then revert back to the default spawning logic 
	// (i.e. selecting a random spawn point)
	do
	{
		if (pSpot)
		{
			bool bValidSpot = true;

			// Check line of sight if enabled
			if (sv_spawnpoint_lineofsight.GetBool())
			{
				CBasePlayer* pPlayer = NULL;
				while ((pPlayer = UTIL_PlayerByIndex(pPlayer ? pPlayer->entindex() + 1 : 1)) != NULL)
				{
					if (pPlayer == this)
						continue;

					trace_t tr;
					UTIL_TraceLine(pSpot->GetAbsOrigin(), pPlayer->GetAbsOrigin(), MASK_SOLID_BRUSHONLY, pSpot, COLLISION_GROUP_NONE, &tr);

					if (tr.fraction == 1.0) // Direct line of sight
					{
						// Msg("Direct line of sight, spawning elsewhere\n");
						bValidSpot = false;
						break;
					}
				}
			}

			// Check player radius if enabled
			if (bValidSpot && sv_spawnradius.GetFloat() > 0)
			{
				CBasePlayer* pPlayer = NULL;
				float flRadiusSquared = sv_spawnradius.GetFloat() * sv_spawnradius.GetFloat();

				while ((pPlayer = UTIL_PlayerByIndex(pPlayer ? pPlayer->entindex() + 1 : 1)) != NULL)
				{
					if (pPlayer == this)
						continue;

					if ((pPlayer->GetAbsOrigin() - pSpot->GetAbsOrigin()).LengthSqr() < flRadiusSquared)
					{
						// Msg("Within radius, spawning elsewhere\n");
						bValidSpot = false;
						break;
					}
				}
			}

			// If both checks passed, spawn here
			if (bValidSpot && g_pGameRules->IsSpawnPointValid(pSpot, this))
			{
				if (pSpot->GetLocalOrigin() != vec3_origin)
				{
					goto ReturnSpot;
				}
			}
		}

		// Move to next spawn point
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	} while (pSpot != pFirstSpot); // loop if we're not back to the start

	// Fall back to Valve's default if all spawn points failed checks
	if (pSpot && (TEAM_SPECTATOR != GetPlayerInfo()->GetTeamIndex()))
	{
		goto ReturnSpot;
	}

ReturnSpot:

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			g_pLastCombineSpawn = pSpot;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			g_pLastRebelSpawn = pSpot;
		}
	}

	if (!pSpot)
	{
		// use the player start entity 
		// if nothing else is found (prevents crashes)
		pSpot = gEntList.FindEntityByClassname(pSpot, "info_player_start");

		if (pSpot)
			goto ReturnSpot;
		else
			return CBaseEntity::Instance(INDEXENT(0));
	}

	g_pLastSpawn = pSpot;

	m_flSlamProtectTime = gpGlobals->curtime + 0.9;

	return pSpot;
}

extern bool g_bWasGamePausedOnJoin;

void CHL2MP_Player::FirstThinkAfterSpawn()
{
	if (HasFirstTimeSpawned())
		return;

	SetFirstTimeSpawned(true);

	if (sv_showhelpmessages.GetBool())
	{
		UTIL_PrintToClient(this, CHAT_CONTEXT "Type " CHAT_LIGHTBLUE "!help " CHAT_CONTEXT "to view chat commands.");
	}

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_SPECTATOR)
			UTIL_PrintToClient(this, CHAT_CONTEXT "You are on team " CHAT_SPEC "%s1", GetTeam()->GetName());
		else if (GetTeamNumber() == TEAM_COMBINE)
			UTIL_PrintToClient(this, CHAT_CONTEXT "You are on team " CHAT_BLUE "%s1", GetTeam()->GetName());
		else if (GetTeamNumber() == TEAM_REBELS)
			UTIL_PrintToClient(this, CHAT_CONTEXT "You are on team " CHAT_RED "%s1", GetTeam()->GetName());
	}

	if (!engine->IsPaused() && g_bWasGamePausedOnJoin)
	{
		engine->GetIServer()->SetPaused(true);
		g_bWasGamePausedOnJoin = false;

		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer)
			{
				pPlayer->RemoveFlag(FL_FROZEN);
			}
		}
	}

	// Remove this think context after it runs
	SetThink(nullptr);
}

void CHL2MP_Player::EnableSpawnProtection()
{
	SetRenderMode(kRenderTransAlpha);
	SetRenderColorA(125);

	m_bSpawnProtected = true;

	SetContextThink(&CHL2MP_Player::DisableSpawnProtection, gpGlobals->curtime + mp_spawnprotection_time.GetFloat(), "DisableSpawnProtectionContext");
}

void CHL2MP_Player::DisableSpawnProtection()
{
	SetRenderColorA(255);

	m_bSpawnProtected = false;

	SetContextThink(NULL, 0, "DisableSpawnProtectionContext");
}

void CHL2MP_Player::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	if (!IsBot())
		LoadPlayerSettings();

	if (engine->IsPaused())
	{
		engine->GetIServer()->SetPaused(false);
		g_bWasGamePausedOnJoin = true;

		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer)
			{
				pPlayer->AddFlag(FL_FROZEN);
			}
		}
	}

#if !defined(NO_STEAM)
	uint64 thisSteamID = GetSteamIDAsUInt64();
	const CEntInfo* pInfo = gEntList.FirstEntInfo();

	for ( ; pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity* pEntity = ( CBaseEntity* ) pInfo->m_pEntity;
		if ( !pEntity )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}
		if ( pEntity->ClassMatches( "npc_satchel" ) )
		{
			CSatchelCharge* pSatchel = dynamic_cast< CSatchelCharge* >( pEntity );
			if ( pSatchel && pSatchel->m_bIsLive && !pSatchel->GetThrower() && pSatchel->GetSteamID() == thisSteamID )
			{
				pSatchel->SetThrower( this );
			}
		}
		else if ( pEntity->ClassMatches( "npc_tripmine" ) )
		{
			CTripmineGrenade* pMine = dynamic_cast< CTripmineGrenade* >( pEntity );
			if ( pMine && pMine->m_bIsLive && !pMine->GetThrower() && pMine->GetSteamID() == thisSteamID )
			{
				pMine->SetThrower( this );
				pMine->m_hOwner = this;
			}
		}
	}
#endif
}


CON_COMMAND( timeleft, "prints the time remaining in the match" )
{
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_GetCommandClient() );

	int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();
	int iFragLimit = fraglimit.GetInt();

	if (iTimeRemaining == 0 && iFragLimit == 0)
	{
		ClientPrint(pPlayer, HUD_PRINTTALK, "No time limit for this game.");
	}
	else if (iTimeRemaining > 0 && iFragLimit > 0)
	{
		int iDays, iHours, iMinutes, iSeconds, iFrags;

		iMinutes = (iTimeRemaining / 60) % 60;
		iSeconds = iTimeRemaining % 60;
		iHours = (iTimeRemaining / 3600) % 24;
		// Yes, this is ridiculous
		iDays = (iTimeRemaining / 86400);
		iFrags = iFragLimit;

		char days[8];
		char hours[8];
		char minutes[8];
		char seconds[8];
		char frags[8];
		char stime[128];

		Q_snprintf(days, sizeof(days), "%2.2d", iDays);
		Q_snprintf(hours, sizeof(hours), "%2.2d", iHours);
		Q_snprintf(minutes, sizeof(minutes), "%2.2d", iMinutes);
		Q_snprintf(seconds, sizeof(seconds), "%2.2d", iSeconds);
		Q_snprintf(frags, sizeof(frags), "%d", iFrags);

		if (iTimeRemaining >= 86400)
			Q_snprintf(stime, sizeof(stime), "Time left in map: %2.2d:%2.2d:%2.2d:%2.2d, or after a player reaches %s3 frags", iDays, iHours, iMinutes, iSeconds, frags);
		else if (iTimeRemaining >= 3600)
			Q_snprintf(stime, sizeof(stime), "Time left in map: %2.2d:%2.2d:%2.2d, or after a player reaches %s3 frags", iHours, iMinutes, iSeconds, frags);
		else
			Q_snprintf(stime, sizeof(stime), "Time left in map: %2.2d:%2.2d, or after a player reaches %s3 frags", iMinutes, iSeconds, frags);

		ClientPrint(pPlayer, HUD_PRINTTALK, stime);
	}
	else
	{
		int iDays, iHours, iMinutes, iSeconds;

		iMinutes = (iTimeRemaining / 60) % 60;
		iSeconds = iTimeRemaining % 60;
		iHours = (iTimeRemaining / 3600) % 24;
		// Yes, this is ridiculous
		iDays = (iTimeRemaining / 86400);

		char days[8];
		char hours[8];
		char minutes[8];
		char seconds[8];
		char stime[128];

		Q_snprintf(days, sizeof(days), "%2.2d", iDays);
		Q_snprintf(hours, sizeof(hours), "%2.2d", iHours);
		Q_snprintf(minutes, sizeof(minutes), "%2.2d", iMinutes);
		Q_snprintf(seconds, sizeof(seconds), "%2.2d", iSeconds);

		if (iTimeRemaining >= 86400)
			Q_snprintf(stime, sizeof(stime), "Time left in map: %2.2d:%2.2d:%2.2d:%2.2d", iDays, iHours, iMinutes, iSeconds);
		else if (iTimeRemaining >= 3600)
			Q_snprintf(stime, sizeof(stime), "Time left in map: %2.2d:%2.2d:%2.2d", iHours, iMinutes, iSeconds);
		else
			Q_snprintf(stime, sizeof(stime), "Time left in map: %2.2d:%2.2d", iMinutes, iSeconds);

		ClientPrint(pPlayer, HUD_PRINTTALK, stime);
	}
}


void CHL2MP_Player::Reset()
{	
	ResetDeathCount();
	ResetFragCount();
}

void CHL2MP_Player::State_Transition( HL2MPPlayerState newState )
{
	State_Leave();
	State_Enter( newState );
}


void CHL2MP_Player::State_Enter( HL2MPPlayerState newState )
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo( newState );

	// Initialize the new state.
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
		(this->*m_pCurStateInfo->pfnEnterState)();
}


void CHL2MP_Player::State_Leave()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}


void CHL2MP_Player::State_PreThink()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnPreThink )
	{
		(this->*m_pCurStateInfo->pfnPreThink)();
	}
}


CHL2MPPlayerStateInfo *CHL2MP_Player::State_LookupInfo( HL2MPPlayerState state )
{
	// This table MUST match the 
	static CHL2MPPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CHL2MP_Player::State_Enter_ACTIVE, NULL, &CHL2MP_Player::State_PreThink_ACTIVE },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CHL2MP_Player::State_Enter_OBSERVER_MODE,	NULL, &CHL2MP_Player::State_PreThink_OBSERVER_MODE }
	};

	for ( int i=0; i < ARRAYSIZE( playerStateInfos ); i++ )
	{
		if ( playerStateInfos[i].m_iPlayerState == state )
			return &playerStateInfos[i];
	}

	return NULL;
}

bool CHL2MP_Player::StartObserverMode(int mode)
{
	//we only want to go into observer mode if the player asked to, not on a death timeout
	if ( m_bEnterObserver == true )
	{
		VPhysicsDestroyObject();
		return BaseClass::StartObserverMode( mode );
	}
	return false;
}

void CHL2MP_Player::StopObserverMode()
{
	m_bEnterObserver = false;
	BaseClass::StopObserverMode();
}

void CHL2MP_Player::State_Enter_OBSERVER_MODE()
{
	int observerMode = m_iObserverLastMode;
	if ( IsNetClient() )
	{
		const char *pIdealMode = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_spec_mode" );
		if ( pIdealMode )
		{
			observerMode = atoi( pIdealMode );
			if ( observerMode <= OBS_MODE_FIXED || observerMode > OBS_MODE_ROAMING )
			{
				observerMode = m_iObserverLastMode;
			}
		}
	}
	m_bEnterObserver = true;
	StartObserverMode( observerMode );
}

void CHL2MP_Player::State_PreThink_OBSERVER_MODE()
{
	// Make sure nobody has changed any of our state.
	//	Assert( GetMoveType() == MOVETYPE_FLY );
	Assert( m_takedamage == DAMAGE_NO );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );
	//	Assert( IsEffectActive( EF_NODRAW ) );

	// Must be dead.
	Assert( m_lifeState == LIFE_DEAD );
	Assert( pl.deadflag );
}


void CHL2MP_Player::State_Enter_ACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	
	// md 8/15/07 - They'll get set back to solid when they actually respawn. If we set them solid now and mp_forcerespawn
	// is false, then they'll be spectating but blocking live players from moving.
	// RemoveSolidFlags( FSOLID_NOT_SOLID );
	
	m_Local.m_iHideHUD = 0;
}


void CHL2MP_Player::State_PreThink_ACTIVE()
{
	//we don't really need to do anything here. 
	//This state_prethink structure came over from CS:S and was doing an assert check that fails the way hl2dm handles death
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2MP_Player::CanHearAndReadChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console unless we're ignoring all chat
	if ( !pPlayer )
		return false;

	return true;
}
