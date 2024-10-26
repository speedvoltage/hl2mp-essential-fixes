
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "filesystem.h"

#ifdef CLIENT_DLL
#define CWeapon357 C_Weapon357
#endif

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

class CWeapon357 : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeapon357, CBaseHL2MPCombatWeapon);
public:

	CWeapon357(void);

	void	PrimaryAttack(void);
	void ItemBusyFrame(void);
	void ItemPostFrame(void);
	bool Holster(CBaseCombatWeapon* pSwitchingTo);
	void CheckZoomToggle(void);
	void ToggleZoom(void);
	void DisableWeaponZoom();
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:

	CWeapon357(const CWeapon357&);
	CNetworkVar(bool, m_bInZoom);
};

IMPLEMENT_NETWORKCLASS_ALIASED(Weapon357, DT_Weapon357)

BEGIN_NETWORK_TABLE(CWeapon357, DT_Weapon357)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeapon357)
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_357, CWeapon357);
PRECACHE_WEAPON_REGISTER(weapon_357);


#ifndef CLIENT_DLL
acttable_t CWeapon357::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};



IMPLEMENT_ACTTABLE(CWeapon357);

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357(void)
{
	m_bReloadsSingly = false;
	m_bFiresUnderwater = false;
	m_bInZoom = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

	m_iClip1--;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	FireBulletsInfo_t info(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;

	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets(info);

	pPlayer->ViewPunch(QAngle(-8, random->RandomFloat(-2, 2), 0));

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}
}

void CWeapon357::ItemBusyFrame(void)
{
	// Allow zoom toggling even when we're reloading
	CheckZoomToggle();
}

void CWeapon357::CheckZoomToggle(void)
{
#ifndef CLIENT_DLL
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer->m_afButtonPressed & IN_ATTACK2 && pPlayer->Is357ZoomEnabled())
	{
		ToggleZoom();
	}
#endif
}

void CWeapon357::ItemPostFrame(void)
{
	// Allow zoom toggling
	CheckZoomToggle();

	BaseClass::ItemPostFrame();
}

#ifndef CLIENT_DLL
void CWeapon357::DisableWeaponZoom()
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer)
	{
		pPlayer->SetWeaponZoomActive(false);
	}
}
#endif

void CWeapon357::ToggleZoom(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

#ifndef CLIENT_DLL

	if (pPlayer->IsSuitZoomActive())
	{
		return;
	}

	int zoomLevel = pPlayer->Get357ZoomLevel();  // Retrieve zoom level from the player

	if (m_bInZoom)
	{
		if (pPlayer->SetFOV(this, 0, 0.2f))
		{
			WeaponSound(SPECIAL2);
			pPlayer->SetCustomFOV(this, pPlayer->GetStoredCustomFOV());
			pPlayer->SetDefaultFOV(pPlayer->GetCustomFOV());
			m_bInZoom = false;
			// pPlayer->SetWeaponZoomActive(false);
			SetContextThink(&CWeapon357::DisableWeaponZoom, gpGlobals->curtime + 0.2f, "DisableZoomContext");
		}
		pPlayer->ShowViewModel( true );
	}
	else
	{
		pPlayer->SetStoredCustomFOV(pPlayer->GetFOV());
		pPlayer->SetDefaultFOV(70);
		pPlayer->SetWeaponZoomActive(true);
		if (pPlayer->SetFOV(this, zoomLevel, 0.2f))
		{
			m_bInZoom = true;
		}
		pPlayer->ShowViewModel( false );
	}
#endif
}

bool CWeapon357::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	if (m_bInZoom)
	{
		ToggleZoom();
	}

	return BaseClass::Holster(pSwitchingTo);
}