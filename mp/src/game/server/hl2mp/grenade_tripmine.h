//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_TRIPMINE_H
#define GRENADE_TRIPMINE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "hl2mp/weapon_slam.h"

class CBeam;


class CTripmineGrenade : public CBaseGrenade, public CSteamIDWeapon
{
public:
	DECLARE_CLASS( CTripmineGrenade, CBaseGrenade );

	CTripmineGrenade();
	void Spawn( void );
	void Precache( void );

	void PowerUp(void);
	void BeamBreakThink( void );
	void DelayDeathThink( void );
	void Event_Killed( const CTakeDamageInfo &info );
	void AttachToEntity( const CBaseEntity* entity );

	void MakeBeam( void );
	void KillBeam( void );

public:
	EHANDLE		m_hOwner;

private:
	Vector		m_vecDir;
	Vector		m_vecEnd;
	float		m_flBeamLength;

	CBeam		*m_pBeam;
	Vector		m_posOwner;
	Vector		m_angleOwner;

	const CBaseEntity* m_pAttachedObject;
	Vector m_vecOldPosAttachedObject;
	QAngle m_vecOldAngAttachedObject;
	DECLARE_DATADESC();
};

#endif // GRENADE_TRIPMINE_H
