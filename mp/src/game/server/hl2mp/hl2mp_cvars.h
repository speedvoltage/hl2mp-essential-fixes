//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL2MP_CVARS_H
#define HL2MP_CVARS_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_INTERMISSION_TIME 120

extern ConVar mp_ready_signal;
extern ConVar mp_restartround;
extern ConVar mp_noweapons;
extern ConVar mp_suitvoice;
extern ConVar sv_custom_sounds;
extern ConVar sv_lockteams;
extern ConVar sv_teamsmenu;
extern ConVar sv_showhelpmessages;
extern ConVar mp_restartgame_notimelimitreset;
extern ConVar mp_spawnprotection;
extern ConVar mp_spawnprotection_time;
extern ConVar mp_afk;
extern ConVar mp_afk_time;
extern ConVar mp_afk_warnings;
extern ConVar sv_teamkill_kick;
extern ConVar sv_teamkill_kick_threshold;
extern ConVar sv_teamkill_kick_warning;

#endif //HL2MP_CVARS_H